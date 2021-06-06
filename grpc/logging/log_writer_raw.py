import struct
import socket
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
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect('/tmp/logging.sock')
    def write_log(self, level, message):

        req = message_pb2.LogMessage(
            level=level.value,
            utc_timestamp = int(datetime.datetime.utcnow().timestamp()),
            msg=message,
            pid = os.getpid(),
            thread=threading.current_thread().ident,
        )
        req_bytes = req.SerializeToString()
        req_len_bytes = struct.pack('!I', len(req_bytes))
        self.sock.sendall(req_len_bytes)
        self.sock.sendall(req_bytes)
        return

if __name__ == '__main__':
    logger = Logger()
    logger.write_log(LogLevel.INFO, 'hello')
