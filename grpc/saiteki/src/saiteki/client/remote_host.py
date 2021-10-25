"""Remote host for performing optimization on an objective function."""

import grpc
import logging
import saiteki

LOGGER = logging.getLogger(__name__)


class RemoteHost:
    """Remote host used for evaluating an objective function."""

    def __init__(self, address, credentials=None):
        """Construct a remote host.

        Arguments:
            address: The address of the remote host as a string.
            credentials: Credentials used to negotiating a connectio with the
                remote host. This should be a `grpc.ChannelCredentials` object
                returned by calling `grpc.ssl_channel_credentials`. May be None.
        """
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

    async def objective_function(self, request, timeout=None):
        """Evaluate the objective function on this remote host.

        Arguments:
            request: A `saiteki.protobuf.CandidateRequest` object.
            timeout: The deadline for the RPC in seconds. May be None.

        Returns: A `saiteki.protobuf.CandidateResponse` object.
        """
        return await self._stub.ObjectiveFunction(request, timeout=timeout)

    async def shutdown(self):
        """Shutdown remote host.

        Returns: Boolean indicating whether the shutdown request was honored.
        """
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
        """Check connectivity state of the underlying channel to remote host.

        Returns: A `grpc.ChannelConnectivity` object representing the current state of the channel.
        """
        return self._channel.get_state()

    @property
    def address(self):
        """Address of the remote host."""
        return self._address
