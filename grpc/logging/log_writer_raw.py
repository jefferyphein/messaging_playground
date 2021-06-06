import struct
import socket
import datetime
import threading
import os
from  enum import Enum
import random
import time

import lorem

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
        print("Writing %s bytes"%len(req_bytes))
        bytes_sent = self.sock.sendall(req_len_bytes)
        # print("Wrote %s bytes"%bytes_sent)
        bytes_sent = self.sock.sendall(req_bytes)
        # print("Data sent %s bytes"%bytes_sent)
        return

if __name__ == '__main__':
    logger = Logger()
    text = lorem.text()
    while True:
        start = random.randint(0, len(text)-1)
        end = random.randint(start, len(text)-1)
        logger.write_log(LogLevel.INFO, text[start:end])
        time.sleep(1)
