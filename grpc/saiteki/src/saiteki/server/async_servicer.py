import logging
import asyncio
import signal
import saiteki

LOGGER = logging.getLogger(__name__)

class AsyncSaitekiServicer(saiteki.protobuf.SaitekiServicer):
    def __init__(self, shutdown_callback):
        self._shutdown_callback = shutdown_callback

    async def Shutdown(self, request, context):
        signal.raise_signal(signal.SIGTERM)
        return saiteki.protobuf.ShutdownResponse()
