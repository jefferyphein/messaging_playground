"""Base class for the asynchronous optimizer."""

import logging
import grpc
import abc
import asyncio
import random
from functools import partial
import saiteki.nevergrad

LOGGER = logging.getLogger(__name__)


class AsyncOptimizationClientBase:
    """Base class for the asynchronous optimizer."""

    def __init__(self, parameters, remote_hosts=list(), shutdown_remote_hosts=False,
                 limit=0, deadline=0.0, threshold=0.0,
                 key=None, cert=None, cacert=None, *args, **kwargs):
        """Construct a basic optimization client.

        Arguments:
            parmaeters: A `saiteki.core.Parameters` object.
            remote_hosts: A list of remote host addresses as strings.
            shutdown_remote_hosts: Boolean indicating whether to send shutdown
                requests to remote hosts when this client is shutdown.
            limit: The maximum number of outstanding evaluations allowed.
            deadline: The deadline for an evaluation. If deadline <= 0, there
                will be no deadline and the evaluation is affored an unbounded
                amount of time to finish. If evaluation is not completed by the
                deadline, it's score will be set to Infinity.
            threshold: Do not submit additional requests once a score below
                this value has been achieved. If threshold <= 0, all
                evaluations within the budget will be submitted.
            key: Filename containing client private key. May be None.
            cert: Filename containing client root certificate. May be None.
            cacert: Filename containing certificate chain. May be None.
        """
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
        self._remote_hosts = list(saiteki.client.RemoteHost(address, credentials) for address in remote_hosts)
        self._limit_semaphore = asyncio.Semaphore(limit) if limit > 0 else None
        self._limit = limit if limit > 0 else None
        self._deadline = deadline if deadline > 0.0 else None
        self._threshold = threshold if threshold > 0.0 else None
        self._shutdown = asyncio.Event()
        self._shutdown_remote_hosts = shutdown_remote_hosts

        self._best_candidate = None
        self._best_score = float('inf')

    @abc.abstractmethod
    async def optimize(self, *args, **kwargs):
        """Optimize."""
        pass

    @abc.abstractmethod
    async def run(self, *args, **kwargs):
        """Run the optimizer."""
        pass

    def update_best_candidate(self, candidate_dict, score, force=False):
        """Update the best candidate, if it's actually an improvement.

        Arguments:
            candidate_dict: A dictionary containing key-value pairs of the
                candidate under consideration.
            score: The candidate's score.
            force: A boolean indicating that the candidate should be updated
                regardless of whether the score was an improvement or not.

            Returns: A boolean indicating whether the best candidate was updated.
        """
        if score < self._best_score or force:
            self._best_score = score
            self._best_candidate_dict = candidate_dict
            self.parameters.update_start_candidate(candidate_dict)

            LOGGER.info("Updated score: %f", score)
            return True

        return False

    async def submit_candidate(self, candidate_dict, context):
        """Submit a candidate for evaluation.

        Arguments:
            candidate_dict: A dictionary containing the key-value pairs of the
                candidate to be evaluated.
            context: A `saiteki.core.OptimizationContext` object.

        Returns: A task corresponding to the scheduled asynchronous evaluation.
            May return None, indicating that no evaluation was scheduled.
        """
        # Block while resources are in use.
        if self._limit_semaphore:
            await self._limit_semaphore.acquire()

        # Do not allow further submissions once client has been shut down.
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
        """Shutdown the client.

        Shutdown the client. This call may also issue remote shutdown requests
        to all configured remote hosts if the `shutdown_remote_hosts` parameter
        was passed to the constructor.
        """
        if not self._shutdown.is_set():
            LOGGER.critical("Shutting down optimization client...")
            self._shutdown.set()

        if self._shutdown_remote_hosts:
            tasks = list(asyncio.create_task(remote_host.shutdown()) for remote_host in self._remote_hosts)
            try:
                await asyncio.gather(*tasks)
            except grpc.aio.AioRpcError as e:
                if e.code() == grpc.StatusCode.UNAVAILABLE:
                    LOGGER.debug("Unable to issue shutdown to remote host due to it being unavailable. Oh well.")
            except Exception:
                LOGGER.exception("Uncaught exception.")
                raise

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
                    LOGGER.debug(
                        "Remote host resource exhausted, trying next remote host (address: %s)",
                        remote_host.address
                    )
                elif e.code() == grpc.StatusCode.UNAVAILABLE:
                    LOGGER.debug(
                        "Remote host unavailable, trying next remote host (address: %s)",
                        remote_host.address
                    )
                elif e.code() == grpc.StatusCode.DEADLINE_EXCEEDED:
                    LOGGER.warn(
                        "RPC deadline exceeded (address: %s, deadline: %.2f)",
                        remote_host.address, self._deadline
                    )
                    break
                elif e.code() == grpc.StatusCode.INVALID_ARGUMENT:
                    LOGGER.warn("Invalid argument: %s", e.details())
                    break
            except Exception:
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
