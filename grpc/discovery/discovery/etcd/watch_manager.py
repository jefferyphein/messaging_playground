import asyncio
import grpc
import logging
import discovery

class WatchManager:
    def __init__(self, etcd_client, key_prefix, watch_callback):
        self._logger = logging.getLogger("discovery.watch_manager")
        self._etcd_client = etcd_client
        self._key_prefix = key_prefix
        self._watch_task = None
        self._watch_callback = watch_callback

    async def _watch(self):
        while True:
            self._logger.info("Starting watch (prefix: %s)", self._key_prefix)
            call = self._etcd_client.watch(self)

            try:
                async for response in call:
                    await self._watch_callback(response.events)
            except grpc.aio.AioRpcError as e:
                self._logger.critical("No connection to remote host, waiting for channel to by ready again.")
                await self._etcd_client.channel_ready()

    async def start(self):
        self._watch_task = asyncio.create_task(self._watch())

    @property
    def key_prefix(self):
        return self._key_prefix
