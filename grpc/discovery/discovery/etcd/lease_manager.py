import grpc
import logging
import asyncio
import discovery.protobuf

class LeaseManager:
    def __init__(self, etcd_client, lease_key, ttl, keep_alive):
        self._logger = logging.getLogger("discovery.lease_manager")
        self._etcd_client = etcd_client
        self._ttl = ttl
        self._keep_alive = keep_alive
        self._lease_key = lease_key.encode()
        self._keep_alive_task = None

    async def inherit_lease(self):
        # Attempt to inherit the lease provided by the lease key.
        request = discovery.protobuf.RangeRequest(
            key=self._lease_key
        )
        response = await self._etcd_client._kv_stub.Range(request)

        if response.count == 0:
            # No lease stored, return None
            return None

        # Return the inherited lease.
        lease_id = int(response.kvs[0].value)
        self._logger.info("Inheriting lease (lease_id: %s)", lease_id)
        return discovery.etcd.Lease(lease_id, self._etcd_client._lease_stub)

    def _refresh(self, lease_id):
        return discovery.protobuf.LeaseKeepAliveRequest(
            ID=lease_id
        )

    @property
    def lease_id(self):
        return self._lease.lease_id

    async def _refresh_iterator(self):
        while True:
            yield self._refresh(self._lease.lease_id)
            await asyncio.sleep(self._keep_alive)

    async def _lease_keep_alive(self):
        call = self._etcd_client._lease_stub.LeaseKeepAlive(self._refresh_iterator())
        async for response in call:
            if response.TTL == 0:
                self._logger.info("Failed to refresh lease (lease_id: %s)", self._lease.lease_id)
                await self.create_lease()
            else:
                self._logger.info("Refreshed lease (lease_id: %s; ttl: %s)", self._lease.lease_id, self._ttl)

    async def create_lease(self):
        request = discovery.protobuf.LeaseGrantRequest(
            TTL=self._ttl
        )
        response = await self._etcd_client._lease_stub.LeaseGrant(request)
        lease_id = response.ID

        # Store the lease ID, but do not apply the lease. This allows this key
        # to persist beyond lease expiration, allowing for inheritence in case
        # of failure.
        request = discovery.protobuf.PutRequest(
            key=self._lease_key,
            value=str(lease_id).encode(),
            lease=lease_id,
        )
        response = await self._etcd_client._kv_stub.Put(request)

        self._logger.info("Created new lease (lease_id: %s)", lease_id)
        return discovery.etcd.Lease(lease_id, self._etcd_client._lease_stub)

    async def _request_kvs(self, keys):
        request = discovery.protobuf.TxnRequest(
            success=list(
                discovery.protobuf.RequestOp(
                    request_range=discovery.protobuf.RangeRequest(
                        key=key
                    )
                )
                for key in keys
            )
        )
        response = await self._etcd_client._kv_stub.Txn(request)
        if not response.succeeded:
            return dict()

        return {
            item.response_range.kvs[0].key.decode():
                item.response_range.kvs[0].value.decode() for item in response.responses
        }

    async def start(self):
        # Attempt lease inheritence.
        keys = list()
        self._lease = await self.inherit_lease()
        if self._lease is None:
            self._lease = await self.create_lease()
        else:
            ok, keys = await self._lease.is_alive()

        # Start the keep-alive task.
        self._keep_alive_task = asyncio.create_task(self._lease_keep_alive())

        # Query the inherited keys and return the inheritance.
        return await self._request_kvs(keys)

    async def run_forever(self):
        super().run_forever()
        if self._keep_alive_task is not None:
            await self._keep_alive_task
