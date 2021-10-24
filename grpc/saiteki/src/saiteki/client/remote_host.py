import grpc
import logging
import saiteki

LOGGER = logging.getLogger(__name__)


class RemoteHost:
    def __init__(self, address, credentials=None):
        if credentials is None:
            self._channel = grpc.aio.insecure_channel(address)
            security = "insecure"
        else:
            self._channel = grpc.aio.secure_channel(address, credentials)
            security = "secure"

        self._stub = saiteki.protobuf.SaitekiStub(self._channel)
        self._address = address
        self._shutdown = False
        LOGGER.info(
            "Created channel to remote host (status: %s; address: %s)",
            security, self._address
        )

    async def objective_function(self, request, timeout):
        return await self._stub.ObjectiveFunction(request, timeout=timeout)

    async def shutdown(self):
        # Nothing to do if remote host is already shut down.
        if self._shutdown:
            return

        # Issue shutdown request and return status.
        response = await self._stub.Shutdown(
            saiteki.protobuf.ShutdownRequest()
        )
        self._shutdown = response.ok
        LOGGER.debug(
            "Received shutdown response from remote host (address: %s, status: %s)",
            self._address, self._shutdown
        )
        return self._shutdown

    def channel_state(self):
        return self._channel.get_state()

    @property
    def address(self):
        return self._address
