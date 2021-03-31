import queue
from concurrent.futures import Executor, as_completed, ThreadPoolExecutor

import grpc


class grpcPoolExecutor(Executor):
    """
    A concurrent.futures.Executor wrapper for gRPC requests
    """
    def __init__(self, max_workers, channels, service_stub):
        """max_workers - The maximum number of outstanding requests

        channels - list of gRPC channels used to service the requests.
        They need not open and may be shared between different pool.

        service_stub - grpc stub for the service being called. Each
        pool is bound to a single service.

        """
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
        """Submit a callable RPC method to be executed. Returns a future that
will eventually have the results of the RPC.

        fn is not actually every called. What matters is that it have
        the /name/ of a method of the service stub provided when the
        pool was constructed.

        fn - A callable binding to a method of the provided service
        stub. This uses the *EXPERIMENTAL* gRPC interface and could
        break at any time.

        request - a gRPC request to be passed into the method

        """
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
