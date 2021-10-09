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

class EtcdClient:
    def __init__(self, hostname, port, namespace="/discovery/"):
        self.namespace = namespace

        self._channel = grpc.aio.insecure_channel("%s:%d" % (hostname, port))
        self._lease_stub = discovery.protobuf.LeaseStub(self._channel)
        self._kv_stub = discovery.protobuf.KVStub(self._channel)
        self._watch_stub = discovery.protobuf.WatchStub(self._channel)

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

    def breakout_key(self, key):
        if not key.startswith("/discovery/"): return None
        arr = key.split("/")
        if len(arr) != 5: return None
        return arr[2:]

    @_handle_errors_async
    async def get(self, key):
        # Attempt to inherit the lease provided by the lease key.
        request = discovery.protobuf.RangeRequest(
            key=key.encode()
        )
        response = await self._kv_stub.Range(request)
        return response.kvs[0].value if response.count == 1 else None

    @_handle_errors_async
    async def get_prefix(self, key_prefix):
        key = key_prefix.encode()
        request = discovery.protobuf.RangeRequest(
            key=key,
            range_end=discovery.etcd.range_end(key)
        )
        response = await self._kv_stub.Range(request)
        return { kv.key.decode(): kv.value.decode() for kv in response.kvs }

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

    async def _watch_iterator(self, watch_manager):
        key = watch_manager.key_prefix.encode()
        range_end = discovery.etcd.range_end(key)
        yield discovery.protobuf.WatchRequest(
            create_request=discovery.protobuf.WatchCreateRequest(
                key=key,
                range_end=range_end,
            )
        )

    def watch(self, watch_manager):
        return self._watch_stub.Watch(self._watch_iterator(watch_manager))

    def unpack_services(self, kvs):
        services = list()
        for key in kvs:
            arr = self.breakout_key(key)
            if arr is None: continue

            metadata = json.loads(kvs[key])
            services.append(discovery.core.Service(*arr, **metadata))
        return services
