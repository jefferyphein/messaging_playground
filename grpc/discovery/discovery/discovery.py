import os
import grpc
import discovery

class Discovery:
    def __init__(self, host, cert=None):
        # Check if we're using a unix domain socket, and raise an exception if
        # a certificate is also provided.
        uds = host.startswith("unix:")
        if uds and cert is not None:
            raise ValueError("Cannot use certificate with Unix domain socket connection.")

        # Read the cert if it's specified.
        if cert is not None:
            if os.path.exists(cert):
                cert = open(cert, 'rb').read()
            else:
                raise FileNotFoundError("Certificate not found.")

        # Create the channel.
        secure = not uds and cert is not None
        if secure:
            credentials = grpc.ssl_channel_credentials(cert)
            self._channel = grpc.secure_channel(host, credentials)
        else:
            self._channel = grpc.insecure_channel(host)

        # Create the RPC stub.
        self._stub = discovery.protobuf.DiscoveryStub(self._channel)

    def register_service(self, hostname, port, instance, service_type, service_name, ttl, metadata=dict()):
        service_id = discovery.protobuf.ServiceID(
            instance=instance,
            service_type=service_type,
            service_name=service_name,
        )
        request = discovery.protobuf.RegisterServiceRequest(
            service_id=service_id,
            hostname=hostname,
            port=int(port),
            ttl=ttl,
        )

        for key in metadata:
            if not isinstance(key, str):
                raise IndexError("Metadata key must be a string (key: %s)." % key)
            if not isinstance(metadata[key], str):
                raise ValueError("Metadata value must be a string (key: %s)." % key)

            request.metadata.append(
                discovery.protobuf.Metadata(
                    key=key,
                    value=metadata[key]
                )
            )

        response = self._stub.RegisterService(request)

    def unregister_service(self, instance, service_type, service_name):
        service_id = discovery.protobuf.ServiceID(
            instance=instance,
            service_type=service_type,
            service_name=service_name,
        )
        request = discovery.protobuf.UnregisterServiceRequest(
            service_id=service_id,
        )

        response = self._stub.UnregisterService(request)

    def keep_alive(self, instance, service_type, service_name):
        service_id = discovery.protobuf.ServiceID(
            instance=instance,
            service_type=service_type,
            service_name=service_name,
        )
        request = discovery.protobuf.KeepAliveRequest(
            service_id=service_id,
        )
        response = self._stub.KeepAlive(request)
