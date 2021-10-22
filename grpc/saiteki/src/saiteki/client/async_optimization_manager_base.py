import logging
import grpc
import abc
import asyncio
import random
from functools import partial
import saiteki.nevergrad

LOGGER = logging.getLogger(__name__)

class AsyncOptimizationManagerBase:
    def __init__(self, parameters, hosts=list(), limit=0, deadline=0.0, key=None, cert=None, cacert=None, *args, **kwargs):
        client_key = open(key, "rb").read() if key else None
        client_cert = open(cert, "rb").read() if cert else None
        client_cacert = open(cacert, "rb").read() if cacert else None

        if client_key is not None or client_cert is not None:
            credentials = grpc.ssl_channel_credentials(
                root_certificates=client_cacert,
                private_key=client_key,
                certificate_chain=client_cert,
            )
        else:
            credentials = None

        self.parameters = parameters
        self._servers = list(saiteki.client.RemoteHost(host, credentials) for host in hosts)
        self._semaphore = asyncio.Semaphore(limit) if limit > 0 else None
        self._limit = limit
        self._deadline = deadline if deadline > 0.0 else None

        self._best_candidate = None
        self._best_score = float('inf')

    @abc.abstractmethod
    async def optimize(self, *args, **kwargs):
        pass

    def update_best_candidate(self, candidate_dict, score):
        if score < self._best_score:
            self._best_score = score
            self._best_candidate_dict = candidate_dict
            self.parameters.update_start_candidate(candidate_dict)

            LOGGER.info("Updated score: %f", score)

    async def submit_candidate(self, candidate_dict, context):
        # Block while resources are in use.
        if self._semaphore:
            await self._semaphore.acquire()

        await context.update(1)
        task = asyncio.create_task(self._objective_function(candidate_dict))
        task.add_done_callback(partial(self._objective_function_done, context, asyncio.get_event_loop()))
        return task

    async def _objective_function(self, candidate_dict):
        # Generate protobuf candidate request
        request = self.parameters.protobuf_request(candidate_dict)

        # Randomly order the servers.
        server_ids = list(range(len(self._servers)))
        random.shuffle(server_ids)

        for server_id in server_ids:
            server = self._servers[server_id]
            try:
                response = await server.objective_function(request, self._deadline)
                return response.score
            except grpc.aio.AioRpcError as e:
                if e.code() == grpc.StatusCode.RESOURCE_EXHAUSTED:
                    LOGGER.debug("Remote host resource exhausted, trying next host (host: %s)", server.host)
                elif e.code() == grpc.StatusCode.UNAVAILABLE:
                    LOGGER.debug("Remote host unavailable, trying next host (host: %s)", server.host)
                elif e.code() == grpc.StatusCode.DEADLINE_EXCEEDED:
                    LOGGER.warn("RPC deadline exceeded (host: %s, deadline: %.2f)", server.host, self._deadline)
                    break
            except Exception as e:
                LOGGER.exception("Uncaught exception.")
                raise

        LOGGER.debug("Candidate ignored.")
        return float('inf')

    def _objective_function_done(self, context, loop, task):
        # Release the resource held by this call.
        if self._semaphore:
            self._semaphore.release()

        # Make sure this is the last thing called.
        asyncio.run_coroutine_threadsafe(context.update(-1), loop=loop)
