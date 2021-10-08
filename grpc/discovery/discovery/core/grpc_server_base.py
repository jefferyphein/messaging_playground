import grpc
import logging
import asyncio

class GrpcServerBase:
    def __init__(self, bind_addr, key, cert, *args, **kwargs):
        self.logger = logging.getLogger("discovery.server")
        uds = bind_addr.startswith("unix:")
        if uds and (key is not None or cert is not None):
            if key is not None:
                self.logger.warning("TLS key provided with UDS binding, ignoring.")
            if cert is not None:
                self.logger.warning("TLS cert provided with UDS binding, ignoring.")

        if not uds:
            if key is not None and cert is None:
                raise ValueError("Key provided without a certificate.")
            if key is None and cert is not None:
                raise ValueError("Certificate is provided without a key.")

        self.server = grpc.aio.server()

        # Should we use a secure port?
        secure = not uds and (key is not None and cert is not None)

        if secure:
            credentials = grpc.ssl_server_credentials(((key.read(), cert.read()),))
            key.close()
            cert.close()
            self.server.add_secure_port(bind_addr, credentials)
            status = "secure"
        else:
            self.server.add_insecure_port(bind_addr)
            status = "uds" if uds else "insecure"

        self.logger.info("Service configured (address: %s; status: %s)", bind_addr, status)

    async def shutdown(self, grace=None):
        self.logger.critical("Server shutdown initiated.")
        await self.server.stop(grace=grace)
        self.logger.critical("Server shutdown complete.")

    async def start(self):
        await self.server.start()

    async def run_forever(self):
        await self.server.wait_for_termination()
