from sqlalchemy import Table, Column, Integer, Float, MetaData, create_engine
from sqlalchemy.orm import mapper, sessionmaker
import logging
import saiteki


LOGGER = logging.getLogger(__name__)


class History:
    pass


class Database:
    def __init__(self, parameters, output_filename=None):
        self.output_filename = output_filename

        self.engine = create_engine("sqlite://")
        metadata = MetaData(bind=self.engine)
        table = parameters.sqlite_table(self.engine, "history", metadata)
        metadata.create_all()
        mapper(History, table)
        Session = sessionmaker(bind=self.engine)
        self.session = Session()

    def record_candidate_score(self, candidate_dict, score, id_):
        candidate = History()
        candidate.id = id_
        candidate._score = score
        for key, value in candidate_dict.items():
            setattr(candidate, key, value)
        self.session.add(candidate)
        self.session.commit()

    def write_sqlite(self):
        print("HI")

    def flush_to_disk(self):
        if self.output_filename == None:
            return

        if self.output_filename.endswith(".sqlite"):
            LOGGER.info("Flushing optimization history to disk (format: sqlite; filename: %s", self.output_filename)
            disk_engine = create_engine("sqlite:///"+self.output_filename)
            disk_engine_conn = disk_engine.raw_connection()
            memory_engine_conn = self.engine.raw_connection()
            memory_engine_conn.backup(disk_engine_conn.connection)
            disk_engine_conn.close()
            disk_engine.dispose()
        elif self.output_filename.endswith(".txt"):
            raise NotImplementedError("Flushing to flat text file not yet implemented.")
        elif self.output_filename.endswith(".csv"):
            raise NotImplementedError("Flushing to CSV file not yet implemented.")
        elif self.output_filename.endswith(".yaml"):
            raise NotImplementedError("Flushing to YAML file not yet implemented.")
