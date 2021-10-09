import sqlalchemy
import json
from enum import Enum
from sqlalchemy.orm import sessionmaker

import discovery

class Action(Enum):
    NONE = 0
    ADD = 1
    UPDATE = 2


class Cache:
    instance = sqlalchemy.Column(sqlalchemy.String, primary_key=True)
    service_type = sqlalchemy.Column(sqlalchemy.String, primary_key=True)
    service_name = sqlalchemy.Column(sqlalchemy.String, primary_key=True)
    hostname = sqlalchemy.Column(sqlalchemy.String)
    port = sqlalchemy.Column(sqlalchemy.Integer)
    ttl = sqlalchemy.Column(sqlalchemy.Integer)
    data = sqlalchemy.Column(sqlalchemy.String)


    @staticmethod
    def find(cls, engine, instance, service_type, service_name, session=None):
        if session is None:
            Session = sessionmaker(engine)
            session = Session()

        return session.query(cls).filter_by(
            instance=instance,
            service_type=service_type,
            service_name=service_name,
        )

    def to_service(self):
        return discovery.core.Service(
            instance=self.instance,
            service_type=self.service_type,
            service_name=self.service_name,
            hostname=self.hostname,
            port=self.port,
            ttl=self.ttl,
            **json.loads(self.data, parse_int=str),
        )

    def register(self, engine, cls, session=None):
        commit = session is None
        if session is None:
            Session = sessionmaker(engine)
            session = Session()

        results = session.query(cls).filter_by(
            instance=self.instance,
            service_type=self.service_type,
            service_name=self.service_name,
        )

        # (instance, service_type, service_name) is a unique index.
        assert results.count() == 0 or results.count() == 1

        # Decide which action to take.
        action = Action.NONE
        if results.count() == 1:
            result = results.first()
            if self == result:
                action = Action.NONE
            else:
                action = Action.UPDATE
        else:
            action = Action.ADD

        # Add or update, if necessary.
        if action == Action.ADD or action == Action.UPDATE:
            try:
                if action == Action.ADD:
                    session.add(self)
                elif action == Action.UPDATE:
                    result.update(self)

                if commit:
                    session.commit()

                return True
            except:
                session.rollback()
                return False

        return False


    def update(self, other):
        self.hostname = other.hostname
        self.port = other.port
        self.ttl = other.ttl
        self.data = other.data


    def __eq__(self, other):
        return self.instance == other.instance \
            and self.service_type == other.service_type \
            and self.service_name == other.service_name \
            and self.hostname == other.hostname \
            and self.port == other.port \
            and json.loads(self.data, parse_int=str) == json.loads(other.data, parse_int=str)


class LocalCache(discovery.Base, Cache):
    __tablename__ = "local_cache"

    @staticmethod
    def register_service(service, engine, session=None):
        entry = service.create_cache(LocalCache)
        entry.register(engine, LocalCache, session=session)


class GlobalCache(discovery.Base, Cache):
    __tablename__ = "global_cache"

    @staticmethod
    def register_service(service, engine, session=None):
        entry = service.create_cache(GlobalCache)
        entry.register(engine, GlobalCache, session=session)


sqlalchemy.Index('local_service_index', LocalCache.instance, LocalCache.service_type)
sqlalchemy.Index('global_service_index', GlobalCache.instance, GlobalCache.service_type)
