import grpc
import logging
import sqlalchemy
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
                # Create Service object from event object.
                if event.type == discovery.protobuf.Event.EventType.PUT:
                    key = event.kv.key.decode()
                    value = event.kv.value.decode()
                    service = self._etcd_client.kv_to_service(key, value)
                elif event.type == discovery.protobuf.Event.EventType.DELETE:
                    key = event.kv.key.decode()
                    service = self._etcd_client.kv_to_service(key)
                else:
                    self._logger.error("Ignoring unknown event type (%s)", event.type)
                    continue

                # Look for the service in global cache, if it's valid.
                if service is None: continue
                result = service.find(self._engine, GlobalCache, session=session)

                # Perform action.
                if event.type == discovery.protobuf.Event.EventType.DELETE:
                    if result is not None:
                        session.delete(result)
                        num_updates += 1
                elif event.type == discovery.protobuf.Event.EventType.PUT:
                    updated = service.create_cache(GlobalCache).register(self._engine, GlobalCache, session=session)
                    if updated:
                        num_updates += 1

            # Commit changes.
            if num_updates > 0:
                session.commit()
                self._logger.info("Updated global cache from %s events.", num_updates)
        except Exception as e:
            session.rollback()
            self._logger.error("An error occurred when updating global cache: %s", e)

    def create_session(self):
        Session = sessionmaker(self._engine)
        return Session()

    async def RegisterService(self, request, context):
        service = discovery.core.Service.from_grpc_request(request)
        updated = service.create_cache(LocalCache).register(self._engine, LocalCache)
        if updated:
            self._logger.info("Service added (instance: %s, type: %s, name: %s)", service.instance, service.service_type, service.service_name)
            # Forward update to the global discovery service.
            await self._etcd_client.register_service(service, lease_id=self._lease_manager.lease_id)

        self.reset_service_timeout(service.instance, service.service_type, service.service_name, int(service.ttl))

        return discovery.protobuf.RegisterServiceResponse(ok=True)

    async def UnregisterService(self, request, context):
        session = self.create_session()
        service = discovery.core.Service.from_grpc_request(request)
        result = service.find(self._engine, LocalCache, session=session)

        ok = True
        if result is not None:
            try:
                session.delete(result)
                session.commit()
                self._logger.info("Service deleted (instance: %s, type: %s, name: %s)", service.instance, service.service_type, service.service_name)
                # Forward delete to the global discovery service.
                await self._etcd_client.unregister_service(service)
            except:
                session.rollback()
                ok = False

        return discovery.protobuf.UnregisterServiceResponse(ok=ok)

    async def KeepAlive(self, request, context):
        session = self.create_session()
        service = discovery.core.Service.from_grpc_request(request)
        result = service.find(self._engine, LocalCache, session=session)

        if result is not None:
            self.reset_service_timeout(service.instance, service.service_type, service.service_name, int(result.ttl))

        ok = result is not None
        return discovery.protobuf.KeepAliveResponse(ok=ok)

    async def service_expiration(self, instance, service_type, service_name, ttl):
        await asyncio.sleep(ttl)
        service_id = discovery.protobuf.ServiceID(
            instance=instance,
            service_type=service_type,
            service_name=service_name,
        )
        request = discovery.protobuf.UnregisterServiceRequest(
            service_id=service_id,
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
    def __init__(self, etcd_hostname, etcd_port, etcd_lease_ttl, etcd_lease_keep_alive, etcd_namespace, service_name, sync_interval, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._logger = logging.getLogger("discovery.server")
        self._sync_interval = sync_interval

        # Create Etcd client.
        self._etcd_client = discovery.etcd.EtcdClient(
            etcd_hostname,
            etcd_port,
            namespace=etcd_namespace,
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
            self._etcd_client.namespace,
            watch_callback=self._servicer.watch_callback,
        )

    async def start(self):
        await super().start()

        # Inherit global services.
        global_kvs = await self._etcd_client.get_prefix(self._etcd_client.namespace)
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

        # Launch local->global synchronization.
        task = asyncio.get_event_loop().create_task(self._sync_local_to_global())

    async def _sync_local_to_global(self):
        while True:
            transactions = list()
            Session = sessionmaker(self._engine)
            session = Session()

            for entry in session.query(LocalCache).all():
                # Check whether this local service is a member of the global cache.
                service = entry.to_service()
                result = service.find(self._engine, GlobalCache, session=session)
                if result is not None and result == entry:
                    continue

                # Local cache entry does not match global cache entry. In this
                # scenario, we trust the local cache and send an update to the
                # global service.
                transactions.append(self._etcd_client.service_to_kv(service))

            if len(transactions) > 0:
                response = await self._etcd_client.put_many(transactions, lease_id=self._lease_manager.lease_id)
                if response is None:
                    self._logger.critical("Failed to synchronize local->global cache.")
                    return

            self._logger.info("Successfully synchronized local->global cache.")

            # Sleep for two minutes before attempting synchronization again.
            await asyncio.sleep(self._sync_interval)
