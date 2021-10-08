import grpc
import re
import json
import discovery

class EtcdClient:
    def __init__(self, hostname, port, namespace="/discovery/"):
        self.namespace = namespace

        self._channel = grpc.aio.insecure_channel("%s:%d" % (hostname, port))
        self._lease_stub = discovery.protobuf.LeaseStub(self._channel)
        self._kv_stub = discovery.protobuf.KVStub(self._channel)

    async def register_service(self, request, lease_id=None):
        metadata = { item.key: item.value for item in request.metadata }
        metadata['hostname'] = request.hostname
        metadata['port'] = str(request.port)
        metadata['ttl'] = str(request.ttl)

        key = "/discovery/%s/%s/%s" % (request.instance, request.service_type, request.service_name)
        value = json.dumps(metadata)
        await self.put(key, value, lease_id=lease_id)

    async def unregister_service(self, request):
        key = "/discovery/%s/%s/%s" % (request.instance, request.service_type, request.service_name)
        await self.rm(key)

    async def put(self, key, value, lease_id=None):
        request = discovery.protobuf.PutRequest(
            key=key.encode(),
            value=value.encode(),
            lease=lease_id,
        )
        return await self._kv_stub.Put(request)

    async def rm(self, key):
        request = discovery.protobuf.DeleteRangeRequest(
            key=key.encode()
        )
        return await self._kv_stub.DeleteRange(request)

    def inherited_services(self, inheritance):
        services = list()
        for key in inheritance:
            if not key.startswith("/discovery/"):
                continue
            arr = key.split("/")
            if len(arr) != 5:
                continue
            instance, service_type, service_name = arr[2], arr[3], arr[4]

            metadata = json.loads(inheritance[key])
            services.append(discovery.core.Service(*arr[2:], **metadata))
        return services
