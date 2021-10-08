import grpc
import logging
import sqlalchemy
import json
import asyncio
from sqlalchemy.orm import sessionmaker

import discovery
from discovery.server.cache import LocalCache

class DiscoveryServicer(discovery.protobuf.DiscoveryServicer):
    def __init__(self, engine, etcd_client, lease_manager):
        self._logger = logging.getLogger("discovery.servicer")
        self._engine = engine
        self._etcd_client = etcd_client
        self._lease_manager = lease_manager
        self._task_list = dict()

    def create_session(self):
        Session = sessionmaker(self._engine)
        return Session()

    async def RegisterService(self, request, context):
        metadata = { item.key: item.value for item in request.metadata }
        service = LocalCache(
            instance=request.instance,
            service_type=request.service_type,
            service_name=request.service_name,
            hostname=request.hostname,
            port=request.port,
            ttl=request.ttl,
            data=json.dumps(metadata),
        )

        updated = service.register(self._engine)
        if updated:
            self._logger.info("Service added (instance: %s, type: %s, name: %s)", request.instance, request.service_type, request.service_name)
            # Forward update to the global discovery service.
            await self._etcd_client.register_service(request, lease_id=self._lease_manager.lease_id)

        self.reset_service_timeout(request.instance, request.service_type, request.service_name, request.ttl)

        return discovery.protobuf.RegisterServiceResponse(ok=True)

    async def UnregisterService(self, request, context):
        session = self.create_session()
        results = LocalCache.find(
            self._engine,
            instance=request.instance,
            service_type=request.service_type,
            service_name=request.service_name,
            session=session,
        )
        assert results.count() == 0 or results.count() == 1

        if results.count() == 1:
            try:
                session.delete(results[0])
                session.commit()
                self._logger.info("Service deleted (instance: %s, type: %s, name: %s)", request.instance, request.service_type, request.service_name)
                # Forward delete to the global discovery service.
                await self._etcd_client.unregister_service(request)
                ok = True
            except:
                session.rollback()
                ok = False
        else:
            ok = True

        return discovery.protobuf.UnregisterServiceResponse(ok=ok)

    async def KeepAlive(self, request, context):
        session = self.create_session()
        results = LocalCache.find(
            self._engine,
            instance=request.instance,
            service_type=request.service_type,
            service_name=request.service_name,
            session=session,
        )
        if results.count() == 1:
            result = results[0]
            self.reset_service_timeout(request.instance, request.service_type, request.service_name, result.ttl)

        ok = results.count() == 1
        return discovery.protobuf.KeepAliveResponse(ok=ok)

    async def service_expiration(self, instance, service_type, service_name, ttl):
        await asyncio.sleep(ttl)
        request = discovery.protobuf.UnregisterServiceRequest(
            instance=instance,
            service_type=service_type,
            service_name=service_name,
        )
        await self.UnregisterService(request, None)

    def reset_service_timeout(self, instance, service_type, service_name, ttl):
        task_name = "%s/%s/%s" % (instance, service_type, service_name)
        task = self._task_list.get(task_name, None)
        if task is not None
            if not task.done():
                task.cancel()

        self._task_list[task_name] = asyncio.create_task(self.service_expiration(instance, service_type, service_name, ttl))

class DiscoveryServer(discovery.core.GrpcServerBase):
    def __init__(self, etcd_hostname, etcd_port, etcd_lease_ttl, etcd_lease_keep_alive, service_name, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._logger = logging.getLogger("discovery.server")

        # Create the Etcd client.
        self._etcd_client = discovery.etcd.EtcdClient(
            etcd_hostname,
            etcd_port
        )

        # Create the Etcd lease manager.
        self._service_name = service_name
        self._lease_manager = discovery.etcd.LeaseManager(
            self._etcd_client,
            "/discovery-leases/" + self._service_name,
            etcd_lease_ttl,
            etcd_lease_keep_alive,
        )

        # Start in-memory SQLite3 database in a thread-safe way.
        # See: https://docs.sqlalchemy.org/en/13/dialects/sqlite.html#threading-pooling-behavior
        self._engine = sqlalchemy.create_engine(
            "sqlite://",
            connect_args={'check_same_thread': False},
            poolclass=sqlalchemy.pool.StaticPool,
        )
        discovery.Base.metadata.create_all(self._engine)

        # Added the servicer to the service.
        self._servicer = DiscoveryServicer(self._engine, self._etcd_client, self._lease_manager)
        discovery.protobuf.add_DiscoveryServicer_to_server(
            self._servicer, self.server
        )

    async def start(self):
        await super().start()
        inheritance = await self._lease_manager.start()
        inherited_services = self._etcd_client.inherited_services(inheritance)

        if len(inherited_services) > 0:
            Session = sessionmaker(bind=self._engine)
            session = Session()
            try:
                for service in inherited_services:
                    local_cache = service.create_local_cache()
                    local_cache.register(self._engine, session=session)
                    self._servicer.reset_service_timeout(service.instance, service.service_type, service.service_name, int(service.ttl))
                session.commit()
                self._logger.info("Inherited %s service(s).", len(inherited_services))
            except:
                session.rollback()
                self._logger.info("Failed to inherit services.")
