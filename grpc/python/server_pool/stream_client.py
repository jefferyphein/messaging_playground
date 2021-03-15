import sys
import logging
import asyncio

import grpc

import hello_pb2
import hello_pb2_grpc

class StreamPool:
    def __init__(self, host_list, stub_class, target):
        self.loop = asyncio.get_event_loop()
        self._hosts = host_list
        self.stubs = []
        self._is_running = False
        self._request_queue = asyncio.Queue()
        self._active_reqs = {}
        self.stub_class = stub_class
        self.target = target
        self.STOP = object()

    async def gen_messages(self):
        while self._is_running:
            request = await self._request_queue.get()
            if request is self.STOP:
                return
            yield request

    async def process_results(self, reply_iter):
        async for reply in reply_iter:
            future = self._active_reqs[reply.correlation_id]
            future.set_result(reply)


    async def connect(self):

        async def do_connect(channel):
            await channel.channel_ready()
            return channel

        for c in asyncio.as_completed([do_connect(c) for c in self._hosts]):
            channel = await c
            stub = self.stub_class(channel)
            self._stubs.append(stub)
            reply_iter = getattr(stub, self.target)(self.gen_messages())
            self.process_results(reply_iter)
            self._is_running = True
        return


    def submit(self, request):
        fut = self.loop.create_future()
        self._active_reqs[request.correlation_id] = fut
        await self._request_queue.put((request,fut))
        return fut
