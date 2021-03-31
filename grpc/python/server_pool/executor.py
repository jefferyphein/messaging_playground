import queue
from concurrent.futures import Executor, as_completed, ThreadPoolExecutor

import grpc


class grpcPoolExecutor(Executor):
    def __init__(self, max_workers, channels, service_stub):
        self._channels = channels
        self._max_workers = max_workers
        self._stub = service_stub
        self._is_running = False
        self._channel_queue = queue.Queue()
        self._pool = ThreadPoolExecutor(max_workers)
        # connect to servers
        self._connect()
        self._tasks = []

    def _connect(self, timeout=None):
        "Connect all channels"

        def do_connect(channel):
            return grpc.channel_ready_future(channel)

        for ch,fut in [(c,do_connect(c)) for c in self._channels]:
            # As the channels become ready, add them to the queue of
            # available channels
            self._channel_queue.put(ch)
        return

    def submit(self, fn, request):
        target = fn.__name__

        def submit_task():
            chan = self._channel_queue.get()
            stub = self._stub(chan)
            response = getattr(stub, target)(request)
            return response

        fut = self._pool.submit(submit_task)
        self._tasks.append(fut)
        fut.add_done_callback(lambda r: self._tasks.remove(fut))
        self._tasks.append(fut)

        return fut

    def map(self, func, *interable, timeout=None, chunksize=1):
        pass

    def shutdown(self, wait=True, *, cancel_futures=False):
        pass
