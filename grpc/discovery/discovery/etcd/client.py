import grpc
import re
import json
import asyncio
import discovery

def _handle_errors_async(func):
    async def handle_errors_async(*args, **kwargs):
        try:
            return await func(*args, **kwargs)
        except grpc.aio.AioRpcError as e:
            if e.code() == grpc.StatusCode.UNAVAILABLE:
                return None
    return handle_errors_async

#def _handle_errors(func):
#    def handle_errors(*args, **kwargs):
#        try:
#            return func(*args, **kwargs)
#        except grpc.aio.AioRpcError as e:
#            if e.code() == grpc.StatusCode.UNAVAILABLE:
#                print("UNAVAILABLE")
#                return None
#    return handle_errors

class EtcdClient:
    def __init__(self, hostname, port, namespace="/discovery/"):
        self.namespace = namespace

        self._channel = grpc.aio.insecure_channel("%s:%d" % (hostname, port))
        self._lease_stub = discovery.protobuf.LeaseStub(self._channel)
        self._kv_stub = discovery.protobuf.KVStub(self._channel)

    async def channel_ready(self):
        await self._channel.channel_ready()

    async def register_service(self, request, lease_id=None):
        metadata = { item.key: item.value for item in request.metadata }
        metadata['hostname'] = request.hostname
        metadata['port'] = str(request.port)
        metadata['ttl'] = str(request.ttl)

        key = "/discovery/%s/%s/%s" % (request.instance, request.service_type, request.service_name)
        value = json.dumps(metadata)
        return await self.put(key, value, lease_id=lease_id)

    async def unregister_service(self, request):
        key = "/discovery/%s/%s/%s" % (request.instance, request.service_type, request.service_name)
        return await self.rm(key)

    @_handle_errors_async
    async def get(self, key):
        # Attempt to inherit the lease provided by the lease key.
        request = discovery.protobuf.RangeRequest(
            key=key.encode()
        )
        response = await self._kv_stub.Range(request)
        return response.kvs[0].value if response.count == 1 else None

    @_handle_errors_async
    async def get_many(self, keys):
        request = discovery.protobuf.TxnRequest(
            success=list(
                discovery.protobuf.RequestOp(
                    request_range=discovery.protobuf.RangeRequest(
                        key=key.encode()
                    )
                ) for key in keys
            )
        )
        response = await self._kv_stub.Txn(request)
        if not response.succeeded:
            return dict()

        return {
            item.response_range.kvs[0].key.decode():
                item.response_range.kvs[0].value.decode() for item in response.responses
        }

    @_handle_errors_async
    async def put(self, key, value, lease_id=None):
        request = discovery.protobuf.PutRequest(
            key=key.encode(),
            value=value.encode(),
            lease=lease_id,
        )
        return await self._kv_stub.Put(request)

    @_handle_errors_async
    async def rm(self, key):
        request = discovery.protobuf.DeleteRangeRequest(
            key=key.encode()
        )
        return await self._kv_stub.DeleteRange(request)

    @_handle_errors_async
    async def lease(self, ttl):
        request = discovery.protobuf.LeaseGrantRequest(
            TTL=ttl
        )
        response = await self._lease_stub.LeaseGrant(request)
        return response.ID

    @_handle_errors_async
    async def lease_keys(self, lease_id):
        request = discovery.protobuf.LeaseTimeToLiveRequest(
            ID=lease_id,
            keys=True,
        )
        response = await self._lease_stub.LeaseTimeToLive(request)
        if response.TTL == 0:
            return None
        else:
            return list(key.decode() for key in response.keys)

    async def _lease_refresh_iterator(self, lease_manager):
        while True:
            yield discovery.protobuf.LeaseKeepAliveRequest(
                ID=lease_manager.lease_id
            )
            await asyncio.sleep(lease_manager.keep_alive)

    def lease_keep_alive(self, lease_manager):
        return self._lease_stub.LeaseKeepAlive(self._lease_refresh_iterator(lease_manager))

    def inherited_services(self, inheritance):
        services = list()
        for key in inheritance:
            if not key.startswith("/discovery/"):
                continue
            arr = key.split("/")
            if len(arr) != 5:
                continue

            metadata = json.loads(inheritance[key])
            services.append(discovery.core.Service(*arr[2:], **metadata))
        return services
