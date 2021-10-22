import logging
import threading
import saiteki

from .async_servicer import AsyncSaitekiServicer

LOGGER = logging.getLogger(__name__)

class AsyncServer(saiteki.core.AsyncGrpcServerBase):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    async def start(self):
        await super().start()

        self._servicer = AsyncSaitekiServicer(self.shutdown)
        saiteki.protobuf.add_SaitekiServicer_to_server(
            self._servicer,
            self.server
        )
