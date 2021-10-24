import logging
import grpc
from concurrent.futures import ThreadPoolExecutor  # noqa: F401

from .grpc_server import GrpcServerBase

LOGGER = logging.getLogger(__name__)


class AsyncGrpcServerBase(GrpcServerBase):
    def __init__(self, num_workers, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.server = grpc.aio.server(
            migration_thread_pool=ThreadPoolExecutor(max_workers=num_workers),
            maximum_concurrent_rpcs=num_workers,
        )

        if self.secure:
            self.server.add_secure_port(self.bind_addr, self.credentials)
            status = "secure; client_auth: %s" % ("yes" if self.authentication else "no")
        else:
            self.server.add_insecure_port(self.bind_addr)
            status = "uds" if self.uds else "insecure"

        LOGGER.info(
            "Server configured (status: %s; num_workers: %d, bind_addr: %s)",
            status, num_workers, self.bind_addr
        )

    async def start(self):
        await self.server.start()
        LOGGER.info("Server started.")

    async def run_forever(self):
        await self.server.wait_for_termination()
        LOGGER.info("Server shutdown.")

    async def shutdown(self, grace=None):
        LOGGER.info("Server shutdown initiated.")
        await self.server.stop(grace=grace)
