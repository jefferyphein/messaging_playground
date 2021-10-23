import logging
import grpc
import abc
import asyncio
import random
from functools import partial
import saiteki.nevergrad

LOGGER = logging.getLogger(__name__)

class AsyncOptimizationManagerBase:
    def __init__(self, parameters, hosts=list(), limit=0, deadline=0.0, threshold=0.0, key=None, cert=None, cacert=None, *args, **kwargs):
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
        self._remote_hosts = list(saiteki.client.RemoteHost(address, credentials) for address in hosts)
        self._limit_semaphore = asyncio.Semaphore(limit) if limit > 0 else None
        self._limit = limit if limit > 0 else None
        self._deadline = deadline if deadline > 0.0 else None
        self._threshold = threshold if threshold > 0.0 else None
        self._shutdown = asyncio.Event()

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
            return True

        return False

    async def submit_candidate(self, candidate_dict, context):
        # Block while resources are in use.
        if self._limit_semaphore:
            await self._limit_semaphore.acquire()

        # Do not allow further submissions once manager has been shut down.
        if self._shutdown.is_set():
            return None

        # Do not submit any further candidates since threshold has been met.
        if self._threshold is not None:
            if self._best_score <= self._threshold:
                return None

        await context.acquire()
        task = asyncio.create_task(self._objective_function(candidate_dict))
        task.add_done_callback(partial(self._objective_function_done, context, asyncio.get_event_loop()))
        return task

    async def shutdown(self):
        LOGGER.critical("Shutting down optimization manager...")
        self._shutdown.set()

    async def _objective_function(self, candidate_dict):
        # Generate protobuf candidate request
        request = self.parameters.protobuf_request(candidate_dict)

        # Randomly order the remote hosts.
        remote_host_ids = list(range(len(self._remote_hosts)))
        random.shuffle(remote_host_ids)

        for remote_host_id in remote_host_ids:
            remote_host = self._remote_hosts[remote_host_id]

            try:
                response = await remote_host.objective_function(request, self._deadline)
                return response.score
            except grpc.aio.AioRpcError as e:
                if e.code() == grpc.StatusCode.RESOURCE_EXHAUSTED:
                    LOGGER.debug("Remote host resource exhausted, trying next remote host (address: %s)", remote_host.address)
                elif e.code() == grpc.StatusCode.UNAVAILABLE:
                    LOGGER.debug("Remote host unavailable, trying next remote host (address: %s)", remote_host.address)
                elif e.code() == grpc.StatusCode.DEADLINE_EXCEEDED:
                    LOGGER.warn("RPC deadline exceeded (address: %s, deadline: %.2f)", remote_host.address, self._deadline)
                    break
                elif e.code() == grpc.StatusCode.INVALID_ARGUMENT:
                    LOGGER.warn("Invalid argument: %s", e.details())
                    break
            except Exception as e:
                LOGGER.exception("Uncaught exception.")
                raise

        LOGGER.debug("Unable to submit request to any remote host, ignoring candidate.")
        return float('inf')

    def _objective_function_done(self, context, loop, task):
        # Release the resource held by this call.
        if self._limit_semaphore:
            self._limit_semaphore.release()

        # Make sure this is the last thing called.
        asyncio.run_coroutine_threadsafe(context.release(), loop=loop)
