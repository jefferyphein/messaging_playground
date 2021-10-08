import discovery

class Lease:
    def __init__(self, lease_id, stub):
        self.lease_id = lease_id
        self._stub = stub

    async def is_alive(self):
        request = discovery.protobuf.LeaseTimeToLiveRequest(
            ID=self.lease_id,
            keys=True,
        )
        response = await self._stub.LeaseTimeToLive(request)
        if response.TTL == 0:
            return False, list()
        else:
            return True, response.keys
