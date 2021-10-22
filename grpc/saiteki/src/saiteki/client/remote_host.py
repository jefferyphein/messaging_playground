import grpc
import asyncio
import logging
import saiteki

LOGGER = logging.getLogger(__name__)

class RemoteHost:
    def __init__(self, host, credentials=None):
        if credentials is None:
            self._channel = grpc.aio.insecure_channel(host)
            security = "insecure"
        else:
            self._channel = grpc.aio.secure_channel(host, credentials)
            security = "secure"

        self._stub = saiteki.protobuf.SaitekiStub(self._channel)
        self._host = host
        LOGGER.info("Created channel to remote host (status: %s; host: %s)", security, self._host)

    async def objective_function(self, request, timeout):
        return await self._stub.ObjectiveFunction(request, timeout=timeout)

    @property
    def host(self):
        return self._host
