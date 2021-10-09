import grpc
import logging
import sqlalchemy
import json
import asyncio
from sqlalchemy.orm import sessionmaker
from functools import wraps
from types import CoroutineType, AsyncGeneratorType, FunctionType

import discovery
from discovery.server.cache import Cache, LocalCache, GlobalCache

class DiscoveryServicer(discovery.protobuf.DiscoveryServicer):
    def __init__(self, engine, etcd_client, lease_manager):
        self._logger = logging.getLogger("discovery.servicer")
        self._engine = engine
        self._etcd_client = etcd_client
        self._lease_manager = lease_manager
        self._task_list = dict()

    async def watch_callback(self, events):
        num_updates = 0
        session = self.create_session()
        try:
            for event in events:
                if event.type == discovery.protobuf.Event.EventType.PUT:
                    key = event.kv.key.decode()
                    value = event.kv.value.decode()
                elif event.type == discovery.protobuf.Event.EventType.DELETE:
                    key = event.kv.key.decode()

                result = self._etcd_client.breakout_key(key)
                if result is None: continue
                instance, service_type, service_name = result

                results = Cache.find(
                    GlobalCache,
                    self._engine,
                    instance=instance,
                    service_type=service_type,
                    service_name=service_name,
                    session=session,
                )
                assert results.count() == 0 or results.count() == 1

                if event.type == discovery.protobuf.Event.EventType.DELETE:
                    if results.count() == 1:
                        session.delete(results[0])
                        num_updates += 1
                elif event.type == discovery.protobuf.Event.EventType.PUT:
                    metadata = json.loads(value)
                    hostname = metadata.get('hostname', None)
                    port = metadata.get('port', None)
                    ttl = metadata.get('ttl', None)
                    if 'hostname' in metadata: del metadata['hostname']
                    if 'port' in metadata: del metadata['port']
                    if 'ttl' in metadata: del metadata['ttl']
                    # TODO: Figure out a better way to do this.

                    service = GlobalCache(
                        instance=instance,
                        service_type=service_type,
                        service_name=service_name,
                        hostname=hostname,
                        port=port,
                        ttl=ttl,
                        data=json.dumps(metadata),
                    )
                    updated = service.register(GlobalCache, self._engine, session=session)
                    if updated:
                        num_updates += 1

            if num_updates > 0:
                session.commit()
                self._logger.info("Updated global cache from %s events.", len(events))
        except Exception as e:
            session.rollback()
            self._logger.error("An error occurred when updating global cache: %s", e)

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

        updated = service.register(LocalCache, self._engine)
        if updated:
            self._logger.info("Service added (instance: %s, type: %s, name: %s)", request.instance, request.service_type, request.service_name)
            # Forward update to the global discovery service.
            await self._etcd_client.register_service(request, lease_id=self._lease_manager.lease_id)

        self.reset_service_timeout(request.instance, request.service_type, request.service_name, request.ttl)

        return discovery.protobuf.RegisterServiceResponse(ok=True)

    async def UnregisterService(self, request, context):
        session = self.create_session()
        results = Cache.find(
            LocalCache,
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
        results = Cache.find(
            LocalCache,
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
        if task is not None:
            if not task.done():
                task.cancel()

        self._task_list[task_name] = asyncio.create_task(self.service_expiration(instance, service_type, service_name, ttl))

class DiscoveryServer(discovery.core.GrpcServerBase):
    def __init__(self, etcd_hostname, etcd_port, etcd_lease_ttl, etcd_lease_keep_alive, service_name, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._logger = logging.getLogger("discovery.server")

        # Create Etcd client.
        self._etcd_client = discovery.etcd.EtcdClient(
            etcd_hostname,
            etcd_port
        )

        # Create Etcd lease manager.
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

        # Add servicer to the server.
        self._servicer = DiscoveryServicer(self._engine, self._etcd_client, self._lease_manager)
        discovery.protobuf.add_DiscoveryServicer_to_server(
            self._servicer, self.server
        )

        # Create watch manager.
        self._watch_manager = discovery.etcd.WatchManager(
            self._etcd_client,
            "/discovery/",
            watch_callback=self._servicer.watch_callback,
        )

    async def start(self):
        await super().start()

        # Inherit global services.
        global_kvs = await self._etcd_client.get_prefix("/discovery/")
        if global_kvs is not None:
            global_services = self._etcd_client.unpack_services(global_kvs)
            if len(global_services) > 0:
                Session = sessionmaker(bind=self._engine)
                session = Session()
                try:
                    for service in global_services:
                        GlobalCache.register_service(service, self._engine, session=session)
                    session.commit()
                    self._logger.info("Added %s service(s) to global cache.", len(global_services))
                except:
                    session.rollback()
                    self._logger.error("Failed to import global services.")

        # Inherit local services.
        inherited_kvs = await self._lease_manager.start()
        inherited_services = self._etcd_client.unpack_services(inherited_kvs)
        if len(inherited_services) > 0:
            Session = sessionmaker(bind=self._engine)
            session = Session()
            try:
                for service in inherited_services:
                    LocalCache.register_service(service, self._engine, session=session)
                    self._servicer.reset_service_timeout(service.instance, service.service_type, service.service_name, int(service.ttl))
                session.commit()
                self._logger.info("Inherited %s service(s) to local cache.", len(inherited_services))
            except:
                session.rollback()
                self._logger.error("Failed to inherit local services.")

        # Start watcher.
        await self._watch_manager.start()
