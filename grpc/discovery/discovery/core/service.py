import json
import discovery

class Service:
    def __init__(self, instance, service_type, service_name, hostname, port, ttl, **metadata):
        self.instance = instance
        self.service_type = service_type
        self.service_name = service_name
        self.hostname = hostname
        self.port = port
        self.ttl = ttl
        self.metadata = dict(metadata)

    def create_local_cache(self):
        print(self.metadata)
        return discovery.server.cache.LocalCache(
            instance=self.instance,
            service_type=self.service_type,
            service_name=self.service_name,
            hostname=self.hostname,
            port=self.port,
            ttl=self.ttl,
            data=json.dumps(self.metadata),
        )
