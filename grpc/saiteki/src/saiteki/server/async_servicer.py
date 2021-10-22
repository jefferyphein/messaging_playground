import logging
import asyncio
import signal
import saiteki

LOGGER = logging.getLogger(__name__)

class AsyncSaitekiServicer(saiteki.protobuf.SaitekiServicer):
    def __init__(self):
        pass

    async def Shutdown(self, request, context):
        signal.raise_signal(signal.SIGTERM)
        return saiteki.protobuf.ShutdownResponse()
