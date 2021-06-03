import datetime
import threading
import os
from  enum import Enum

import grpc

import message_pb2_grpc
import message_pb2

class LogLevel(Enum):
    DEBUG = 0
    INFO = 1
    WARNING = 2
    ERROR = 3
    CRITICAL = 4


class Logger:
    def __init__(self):
        self.channel = grpc.insecure_channel('unix:///tmp/logging.sock')
        self.stub = message_pb2_grpc.LoggerStub(self.channel)

    def write_log(self, level, message):

        req = message_pb2.LogMessage(
            level=level.value,
            utc_timestamp = int(datetime.datetime.utcnow().timestamp()),
            msg=message,
            pid = os.getpid(),
            thread=threading.current_thread().ident,
        )
        response = self.stub.WriteLog(req)
        return

if __name__ == '__main__':
    logger = Logger()
    logger.write_log(LogLevel.INFO, "hello")
