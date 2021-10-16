import grpc
import re
import json
import asyncio
import threading
import discovery

def _handle_errors_async(func):
    async def handle_errors_async(*args, **kwargs):
        try:
            return await func(*args, **kwargs)
        except grpc.aio.AioRpcError as e:
            if e.code() == grpc.StatusCode.UNAVAILABLE:
                return None
            else:
                print("Unexpected gRPC error (code: %s, details: %s)" % (e.code(), e.details()))
                raise
    return handle_errors_async

class EtcdClient:
    def __init__(self, hostname, port, namespace="/discovery/", max_txn_ops=128):
        self.namespace = namespace
        if not self.namespace.endswith("/"):
            self.namespace += "/"

        self._channel = grpc.aio.insecure_channel("%s:%d" % (hostname, port))
        self._lease_stub = discovery.protobuf.LeaseStub(self._channel)
        self._kv_stub = discovery.protobuf.KVStub(self._channel)
        self._watch_stub = discovery.protobuf.WatchStub(self._channel)
        self._max_txn_ops = max_txn_ops

    async def channel_ready(self):
        await self._channel.channel_ready()

    async def register_service(self, service, lease_id=None):
        key, value = self.service_to_kv(service)
        return await self.put(key, value, lease_id=lease_id)

    async def unregister_service(self, service):
        key, value = self.service_to_kv(service)
        return await self.rm(key)

    def kv_to_service(self, key, value=None):
        if not key.startswith(self.namespace):
            return None
        arr = key.split("/")
        if len(arr) != 3+self.namespace.count("/"):
            return None
        metadata = json.loads(value, parse_int=str) if value is not None else dict()
        return discovery.core.Service(*arr[2:], **metadata)

    def service_to_kv(self, service):
        key = "%s%s/%s/%s" % (self.namespace, service.instance, service.service_type, service.service_name)
        value = json.dumps(service.extended_metadata(), sort_keys=True)
        return key, value

    @_handle_errors_async
    async def get(self, key):
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
        results = dict()
        for n in range(0, len(keys), self._max_txn_ops):
            request = discovery.protobuf.TxnRequest(
                success=list(
                    discovery.protobuf.RequestOp(
                        request_range=discovery.protobuf.RangeRequest(
                            key=key.encode()
                        )
                    ) for key in keys[n:n+self._max_txn_ops]
                )
            )
            response = await self._kv_stub.Txn(request)

            if not response.succeeded:
                continue

            results.update({
                item.response_range.kvs[0].key.decode():
                    item.response_range.kvs[0].value.decode() for item in response.responses
            })

        return results

    @_handle_errors_async
    async def put(self, key, value, lease_id=None):
        request = discovery.protobuf.PutRequest(
            key=key.encode(),
            value=value.encode(),
            lease=lease_id,
        )
        return await self._kv_stub.Put(request)

    @_handle_errors_async
    async def put_many(self, kv_pairs, lease_id=None):
        responses = list()
        for n in range(0, len(kv_pairs), self._max_txn_ops):
            request = discovery.protobuf.TxnRequest(
                success=list(
                    discovery.protobuf.RequestOp(
                        request_put=discovery.protobuf.PutRequest(
                            key=key.encode(),
                            value=value.encode(),
                            lease=lease_id,
                        )
                    ) for key, value in kv_pairs[n:n+self._max_txn_ops]
                )
            )
            response = await self._kv_stub.Txn(request)
            responses.append(response)
        return responses

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

    def lease_keep_alive(self, lease_manager):
        done_event = threading.Event()

        async def _iterator(_manager):
            while not done_event.is_set():
                yield discovery.protobuf.LeaseKeepAliveRequest(
                    ID=_manager.lease_id
                )
                await asyncio.sleep(_manager.keep_alive)

        def _cancel(*args, **kwargs):
            done_event.set()

        call = self._lease_stub.LeaseKeepAlive(_iterator(lease_manager))
        call.add_done_callback(_cancel)
        return call

    def watch(self, watch_manager):
        async def _iterator(_manager):
            key = _manager.key_prefix.encode()
            range_end = discovery.etcd.range_end(key)
            yield discovery.protobuf.WatchRequest(
                create_request=discovery.protobuf.WatchCreateRequest(
                    key=key,
                    range_end=range_end,
                )
            )

        return self._watch_stub.Watch(_iterator(watch_manager))

    def unpack_services(self, kvs):
        services = list()
        for key,value in kvs.items():
            service = self.kv_to_service(key, value)
            if service is not None:
                services.append(service)
        return discovery.core.ServiceMap(services)
