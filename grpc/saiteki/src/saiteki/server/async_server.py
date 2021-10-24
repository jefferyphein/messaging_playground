import logging
import saiteki

from .async_servicer import SaitekiServicer

LOGGER = logging.getLogger(__name__)


class AsyncServer(saiteki.core.AsyncGrpcServerBase):
    def __init__(self, keep_alive=False, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.keep_alive = keep_alive

    async def start(self):
        await super().start()

        self._servicer = SaitekiServicer(self.keep_alive)
        saiteki.protobuf.add_SaitekiServicer_to_server(
            self._servicer,
            self.server
        )
