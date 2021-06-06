
from concurrent.futures import ThreadPoolExecutor
import asyncio
import struct

import grpc

import message_pb2_grpc
import message_pb2

async def handle_log_message(reader, writer):
    len_bytes = await reader.read(struct.calcsize('!I')) # 4 bytes for the length of the message
    message_len = struct.unpack('!I', len_bytes)[0]
    print("Getting log messag with %s bytes"%message_len)
    msg_bytes = await reader.read(message_len)
    message = message_pb2.LogMessage.FromString(msg_bytes)
    print(message)


async def main():
    server = await asyncio.start_unix_server(
        handle_log_message,
        '/tmp/logging.sock'
    )

    async with server:
        await server.serve_forever()

    return

if __name__ == '__main__':
    asyncio.run(main())
