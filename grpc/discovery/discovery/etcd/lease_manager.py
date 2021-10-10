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
        self._lease_id = None
        self._lease_key = lease_key
        self._keep_alive_task = None

    async def inherit_lease(self):
        lease_id = await self._etcd_client.get(self._lease_key)
        if lease_id is None:
            return None
        lease_id = int(lease_id)

        self._logger.info("Inheriting lease (lease_id: %s)", lease_id)
        return lease_id

    def _refresh(self, lease_id):
        return discovery.protobuf.LeaseKeepAliveRequest(
            ID=lease_id
        )

    @property
    def lease_id(self):
        return self._lease_id

    @property
    def keep_alive(self):
        return self._keep_alive

    async def _lease_keep_alive(self):
        while True:
            self._logger.info("Starting lease keep-alive stream.")
            call = self._etcd_client.lease_keep_alive(self)

            try:
                async for response in call:
                    if response.TTL == 0:
                        self._logger.info("Failed to refresh lease (lease_id: %s)", self._lease_id)
                        await self.create_lease()
                    else:
                        self._logger.info("Refreshed lease (lease_id: %s; ttl: %s)", self._lease_id, self._ttl)
            except grpc.aio.AioRpcError as e:
                self._logger.critical("No connection to remote host, waiting for channel to be ready again.")
                await self._etcd_client.channel_ready()

    async def create_lease(self):
        lease_id = await self._etcd_client.lease(self._ttl)
        if lease_id is None:
            return None
        self._lease_id = lease_id

        await self._etcd_client.put(self._lease_key, str(lease_id), lease_id=lease_id)

        self._logger.info("Created new lease (lease_id: %s)", lease_id)
        return lease_id

    async def start(self):
        # Attempt lease inheritence.
        keys = list()
        self._lease_id = await self.inherit_lease()
        if self._lease_id is None:
            self._lease_id = await self.create_lease()
        else:
            keys = await self._etcd_client.lease_keys(self._lease_id)

        # Start the keep-alive task.
        self._keep_alive_task = asyncio.create_task(self._lease_keep_alive())

        # There is no lease, return empty dictionary.
        if self._lease_id is None:
            return dict()

        # There are no keys, return empty dictionary.
        if keys is None:
            return dict()

        # Query the inherited keys and return the inheritance.
        result = await self._etcd_client.get_many(keys)
        return result if result is not None else dict()
