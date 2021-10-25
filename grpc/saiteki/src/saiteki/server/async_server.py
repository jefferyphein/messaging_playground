"""Asynchronous gRPC optimization server."""

import logging
import saiteki

from .async_servicer import SaitekiServicer

LOGGER = logging.getLogger(__name__)


class AsyncServer(saiteki.core.AsyncGrpcServerBase):
    """Asynchronous gRPC optimization server."""

    def __init__(self, keep_alive=False, *args, **kwargs):
        """Construct a gRPC server.

        Arguments:
            keep_alive: Boolean indicating whether the server should stay alive
                even if a client requests a remote shutdown.
        """
        super().__init__(*args, **kwargs)
        self.keep_alive = keep_alive

    async def start(self):
        """Start the gRPC server."""
        await super().start()

        self._servicer = SaitekiServicer(self.keep_alive)
        saiteki.protobuf.add_SaitekiServicer_to_server(
            self._servicer,
            self.server
        )
