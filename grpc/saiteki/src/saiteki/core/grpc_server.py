"""Base class for gRPC server."""

import grpc
import logging

LOGGER = logging.getLogger(__name__)


class GrpcServerBase:
    """Base class for gRPC server."""

    def __init__(self, bind_addr="[::1]:0", key=None, cert=None, cacert=None, authentication=False, *args, **kwargs):
        """Construct a gRPC base object.

        Arguments:
            bind_addr: The address to which this server will be bound. May be
                any address that gRPC understands, see gRPC documentation for
                details. Defaults to "[::1]:0", which is ipv6 notation for
                localhost on a random port.
            key: Filename containing server private key. May be None.
            cert: Filename containing server certificate. May be None.
            cacert: Filename containing certificate chain. May be None.
            authentication: Boolean indicating whether client authentication
                should be enforced.
        """
        LOGGER = logging.getLogger("saiteki.grpc_server")

        self.bind_addr = bind_addr
        self.authentication = authentication

        self.uds = self.bind_addr.startswith("unix:")
        if self.uds and (key is not None or cert is not None or cacert is not None):
            if key is not None:
                LOGGER.warning("TLS key provided with UDS binding, ignoring.")
            if cert is not None:
                LOGGER.warning("TLS certificate provided with UDS binding, ignoring.")
            if cacert is not None:
                LOGGER.warning("TLS CA certificate provided with UDS binding, ignoring.")

        if not self.uds:
            if key is not None and cert is None:
                raise ValueError("TLS key provided without a certificate.")
            if key is None and cert is not None:
                raise ValueError("TLS certificate provided without a key.")

        # Should we use a secure port?
        self.secure = not self.uds and (key is not None and cert is not None)

        if self.secure:
            server_key = open(key, "rb").read()
            server_cert = open(cert, "rb").read()
            server_cacert = open(cacert, "rb").read() if cacert else None

            self.credentials = grpc.ssl_server_credentials(((server_key, server_cert),), server_cacert, authentication)
