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
                    self._logger.warning("Ignoring unknown event type (%s) in watch callback.", event.type)
                    continue

                # Look for the service in global cache, if it's valid.
                if service is None: continue

                # Perform action.
                if event.type == discovery.protobuf.Event.EventType.DELETE:
                    result = service.find(self._engine, GlobalCache, session=session)
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
    def __init__(self, etcd_hostname, etcd_port, etcd_lease_ttl, etcd_lease_keep_alive, etcd_namespace, etcd_lease_namespace, etcd_max_txn_ops, service_name, sync_interval, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._logger = logging.getLogger("discovery.server")
        self._sync_interval = sync_interval

        # Make sure the lease namespace ends with a forward slash.
        if not etcd_lease_namespace.endswith("/"):
            etcd_lease_namespace += "/"

        # Create Etcd client.
        self._etcd_client = discovery.etcd.EtcdClient(
            etcd_hostname,
            etcd_port,
            namespace=etcd_namespace,
            max_txn_ops=etcd_max_txn_ops,
        )

        # Create Etcd lease manager.
        self._service_name = service_name
        self._lease_manager = discovery.etcd.LeaseManager(
            self._etcd_client,
            etcd_lease_namespace + self._service_name,
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

        # Synchronize global cache with remote cache.
        await self._sync_remote_to_global()

        # Inherit local services.
        inherited_kvs = await self._lease_manager.start()
        inherited_services_map = self._etcd_client.unpack_services(inherited_kvs)
        if len(inherited_services_map) > 0:
            Session = sessionmaker(bind=self._engine)
            session = Session()
            try:
                for key,service in inherited_services_map.items():
                    LocalCache.register_service(service, self._engine, session=session)
                    self._servicer.reset_service_timeout(service.instance, service.service_type, service.service_name, int(service.ttl))
                session.commit()
                self._logger.info("Inherited %s service(s) to local cache.", len(inherited_services_map))
            except:
                session.rollback()
                self._logger.error("Failed to inherit local services.")

        # Start watcher.
        await self._watch_manager.start()

        # Launch local->global synchronization.
        task = asyncio.get_event_loop().create_task(self._synchronize())

    async def _synchronize(self):
        while True:
            # Copy the remote cache into the global cache
            await self._sync_remote_to_global()

            # Copy the local cache into the global cache. This may trigger
            # updates to the remote cache.
            await self._sync_local_to_global()

            # Sleep for the specified interval before synchronizing again.
            await asyncio.sleep(self._sync_interval)

    async def _sync_remote_to_global(self):
        remote_kvs = await self._etcd_client.get_prefix(self._etcd_client.namespace)
        if remote_kvs is None:
            self._logger.error("Failed to synchronize services from remote endpoint (reason: connection failed).")
            return

        remote_services_map = self._etcd_client.unpack_services(remote_kvs)
        Session = sessionmaker(bind=self._engine)
        session = Session()

        try:
            # Any service listed in the remote cache that are not in the global
            # cache will be added to the global cache.
            num_updated = 0
            for key,service in remote_services_map.items():
                updated = GlobalCache.register_service(service, self._engine, session=session)
                if updated:
                    num_updated += 1

            # Any service listed in the global cache that was not in the remote
            # cache will be deleted from the global cache.
            num_deleted = 0
            for entry in session.query(GlobalCache).all():
                service = entry.to_service()
                if service not in remote_services_map:
                    session.delete(entry)
                    num_deleted += 1

            session.commit()
            self._logger.info("Successfully synchronized remote->global cache (updates: %s, deletes: %s)", num_updated, num_deleted)
        except Exception as e:
            session.rollback()
            self._logger.error("Failed to synchronize global cache from remote endpoint (reason: database error).")
            raise e

    async def _sync_local_to_global(self):
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

        self._logger.info("Successfully synchronized local->global cache (num_puts: %s).", len(transactions))
