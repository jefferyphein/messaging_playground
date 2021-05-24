import socket
import os
import sys
import argparse


class _Server:
    def __init__(self, addr):
        self._addr = addr
        self.count = 0

    def __call__(self):
        conn, _ = self._socket.accept()
        while True:
            self.count += 1
            msg = str(self.count).rjust(32, '0')
            try:
                conn.send(msg.encode())
            except ConnectionResetError:
                break
            except BrokenPipeError:
                break
        conn.close()


class IPServer(_Server):
    def __init__(self, addr, port):
        super().__init__(addr)
        self._port = port
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._socket.bind((self._addr, self._port))
        self._socket.listen(0)


class UnixServer(_Server):
    def __init__(self, addr):
        super().__init__(addr)
        self._socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._socket.bind(self._addr)
        self._socket.listen(0)


parser = argparse.ArgumentParser(description="Test IP/Unix sockets on loopback.")
parser.add_argument('--ip', action='store_true', help="Use IP sockets")
parser.add_argument('--unix', action='store_true', help="Use Unix sockets")
args = parser.parse_args(sys.argv[1:])

if args.unix and args.ip:
    raise RuntimeError("Must specify exactly one of --ip or --unix")
elif args.unix:
    socket_file = '/tmp/uds_server.sock'
    if os.path.exists(socket_file):
        os.unlink(socket_file)

    server = UnixServer(socket_file)
elif args.ip:
    server = IPServer('127.0.0.1', 5000)
else:
    raise RuntimeError("Must specify either --ip or --unix")

try:
    server()
except KeyboardInterrupt:
    pass

print(server.count)
