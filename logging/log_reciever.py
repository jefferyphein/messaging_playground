#!/usr/bin/env python

from concurrent.futures import ThreadPoolExecutor
import asyncio

import grpc

import message_pb2_grpc
import message_pb2


class LogReciever(message_pb2_grpc.LoggerServicer):
    def WriteLog(self, request, context):
        print(request)
        return message_pb2.google_dot_protobuf_dot_empty__pb2.Empty()

async def run():
    port = 7106407
    server = grpc.aio.server(ThreadPoolExecutor())
    server.add_insecure_port("unix:///tmp/logging.sock")
    message_pb2_grpc.add_LoggerServicer_to_server(LogReciever(), server)
    await server.start()
    await server.wait_for_termination()

if __name__ == '__main__':
    asyncio.run(run())
