import sys
import os
from concurrent import futures
import time
import threading
import asyncio

import grpc

import hello_pb2
import hello_pb2_grpc

def say_hello(name):
    time.sleep(5)
    return "Hello {}".format(name)


class Greeter(hello_pb2_grpc.GreeterServicer):
    def __init__(self, pid, num_workers=1):
        self.pid = pid
        self.num_workers = num_workers
    def Capacity(self, request, context):
        reply = hello_pb2.CapacityReply(num_workers=self.num_workers)
        return reply
    def SayHello(self, request, context):
        time.sleep(10)
        message = "Hello {} from {}".format(request.name, self.pid)
        reply = hello_pb2.HelloReply(message=message, pid=self.pid)
        return reply
    def StreamHello(self, request_iter, context):
        print(request_iter)
        i=0
        for request in request_iter:
            message = "Hello number {}, {}".format(
                request.name, i)
            reply = hello_pb2.HelloReply(message=say_hello(request.name), pid=self.pid)
            print("yielding response %s"%threading.current_thread())
            yield reply
            i+=1

async def run():
    import logging
    logging.basicConfig()
    server = grpc.aio.server(futures.ThreadPoolExecutor(max_workers=4),
                             maximum_concurrent_rpcs=4)
    hello_pb2_grpc.add_GreeterServicer_to_server(
        Greeter(os.getpid(), 4), server
    )
    port = int(sys.argv[1])
    logging.info("Starting on port %s", port)
    server.add_insecure_port('[::]:{}'.format(port))
    await server.start()
    await server.wait_for_termination()

if __name__ == '__main__':
    asyncio.run(run())
