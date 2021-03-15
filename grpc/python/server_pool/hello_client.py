import sys
import logging
import asyncio

import grpc

import hello_pb2
import hello_pb2_grpc


class ServerPool:
    """Manage a collection of gRPC servers"""
    def __init__(self, host_list, stub):
        self._hosts = host_list
        self.loop = asyncio.get_event_loop()
        self._queue = asyncio.Queue()
        self._is_running = False
        self._stub = stub
        self._tasks = []

    async def connect(self):
        "Connect to all servers"
        async def do_connect(channel):
            await channel.channel_ready()
            return channel

        for channel in asyncio.as_completed([do_connect(c) for c in self._hosts]):
            stub = self._stub(await channel)
            await self._queue.put(stub)
        self._is_running = True
        return

    async def close(self, grace=None):
        "Close connectios to all servers"
        await asyncio.gather(*[c.close(grace) for c in self._hosts])

    async def get_stub(self):
        stub = await self._queue.get()
        return stub

    async def submit(self, target, request, wait=True):
        """Submit a task to the pool.

        Will return after the request has been asigned to a server,
        but before the request has been completed or return
        immediatly, depending on the value of `wait`. Returns a future
        that may be used to access the result.

        ARGUMENTS:

        target The name (as a string) of the gRPC method stub to call

        request Argument Request to pass to stub

        wait Await the assignment of the task to a server before returning. Otherwise return immediatly

        """

        start_ev = asyncio.Event()

        async def submit_task(fut):
            stub = await self.get_stub()
            start_ev.set()
            response = await getattr(stub, target)(request)
            await self._queue.put(stub)
            fut.set_result(response)
            self._tasks.remove(task)

        fut = self.loop.create_future()
        task = self.loop.create_task(submit_task(fut))
        self._tasks.append(task)

        # def cb(r):
        #     # fut.set_result(r)
        #     print("got %s"%r)

        # task.add_done_callback(cb)
        if wait:
            await start_ev.wait()

        return fut


async def gen_messages(num_messages):
    for i in range(num_messages):
        request = hello_pb2.HelloRequest(name="erik%s" % i)
        yield request


async def run():
    host_list = ['localhost:%s' % port for port in sys.argv[1:]]
    pool = ServerPool([grpc.aio.insecure_channel(host) for host in host_list],
                      hello_pb2_grpc.GreeterStub
                      )
    await pool.connect()
    # pool.submit("StreamHello", gen_messages())
    futures = []


    def print_result(fut):
        print(fut.result())


    for i in range(100):
        request = hello_pb2.HelloRequest(name='erik%s' % i)
        fut = await pool.submit('SayHello', request, wait=False)
        # fut.add_done_callback(print_result)
        # response = await fut
        futures.append(fut)
    print("Requests submitted")

    for response in asyncio.as_completed(futures):
        print(await response)

    await pool.close()

if __name__ == '__main__':
    asyncio.run(run())
