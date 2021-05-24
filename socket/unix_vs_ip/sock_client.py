import socket
import time
import argparse
import sys


class _Client:
    def __init__(self, addr):
        self._addr = addr

    def checkpoint(self, msgs, start_time):
        elapsed = time.monotonic() - start_time
        print("Rate: %f msgs/sec" % (msgs / elapsed))

    def start(self, server_addr, num_messages=1024):
        _start = time.monotonic()
        msgs = 0
        sock = socket.socket(self._socket_type, socket.SOCK_STREAM)
        sock.connect(server_addr)

        while True:
            data = sock.recv(32)
            msgs += 1

            if msgs % 2**12 == 0:
                self.checkpoint(msgs, _start)
                print(msgs, data)

        sock.close()
        self.checkpoint(msgs, _start)


class IPClient(_Client):
    def __init__(self, addr, port):
        super().__init__(addr)
        self._port = int(port)
        self._socket_type = socket.AF_INET
        self._server_addr = (self._addr, self._port)

    def start(self, num_messages):
        super().start((self._addr, self._port), num_messages=num_messages)


class UnixClient(_Client):
    def __init__(self, addr):
        super().__init__(addr)
        self._socket_type = socket.AF_UNIX
        self._server_addr = self._addr

    def start(self, num_messages):
        super().start(self._addr, num_messages=num_messages)


parser = argparse.ArgumentParser(description="Test IP/Unix sockets on loopback.")
parser.add_argument('--ip', action='store_true', help="Use IP sockets")
parser.add_argument('--unix', action='store_true', help="Use Uni sockets")
args = parser.parse_args(sys.argv[1:])


if args.unix and args.ip:
    raise RuntimeError("Must specify exactly one of --ip or --unix")
elif args.unix:
    client = UnixClient('/tmp/uds_server.sock')
elif args.ip:
    client = IPClient('127.0.0.1', 5000)
else:
    raise RuntimeError("Must specify either --ip or --unix")

try:
    client.start(num_messages=2**20)
except KeyboardInterrupt:
    pass
