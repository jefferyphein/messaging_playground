import json
import discovery
from sqlalchemy.orm import sessionmaker

class ServiceData:
    def __init__(self, hostname=None, port=None, ttl=None, **metadata):
        self.hostname = hostname
        self.port = port
        self.ttl = ttl
        self.metadata = dict(**metadata)

    def dict(self):
        d = { 'hostname': self.hostname, 'port': self.port, 'ttl': self.ttl }
        return { **d, **self.metadata }

class Service:
    def __init__(self, instance, service_type, service_name, **service_metadata):
        self.instance = str(instance)
        self.service_type = str(service_type)
        self.service_name = str(service_name)
        self.service_data = ServiceData(**service_metadata)

    @property
    def hostname(self):
        return self.service_data.hostname

    @property
    def port(self):
        return self.service_data.port

    @property
    def ttl(self):
        return self.service_data.ttl

    @property
    def metadata(self):
        return self.service_data.metadata

    def extended_metadata(self):
        return self.service_data.dict()

    @staticmethod
    def from_grpc_request(request):
        metadata = { item.key: item.value for item in request.metadata } if hasattr(request, 'metadata') else dict()
        return Service(
            instance=request.service_id.instance,
            service_type=request.service_id.service_type,
            service_name=request.service_id.service_name,
            hostname=getattr(request, 'hostname', None),
            port=getattr(request, 'port', None),
            ttl=getattr(request, 'ttl', None),
            **metadata,
        )

    def create_cache(self, cls):
        return cls(
            instance=self.instance,
            service_type=self.service_type,
            service_name=self.service_name,
            hostname=self.hostname,
            port=self.port,
            ttl=self.ttl,
            data=json.dumps(self.metadata, sort_keys=True),
        )

    def find(self, engine, cls, session=None):
        if session is None:
            Session = sessionmaker(bind=engine)
            session = Session()

        results = cls.find(
            cls,
            engine,
            instance=self.instance,
            service_type=self.service_type,
            service_name=self.service_name,
            session=session,
        )
        assert results.count() == 0 or results.count() == 1
        return results[0] if results.count() == 1 else None
