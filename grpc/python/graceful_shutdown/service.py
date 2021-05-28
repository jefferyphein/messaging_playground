import grpc
import concurrent.futures
import threading
import google.protobuf

import protos.shutdown_pb2
import protos.shutdown_pb2_grpc

class Service:
    def __init__(self):
        self._server = grpc.server(
            concurrent.futures.ThreadPoolExecutor(max_workers=1),
            maximum_concurrent_rpcs=1
        )
        protos.shutdown_pb2_grpc.add_ShutdownServicer_to_server(
            self, self._server
        )
        self._server.add_insecure_port("[::]:46307")

        self._shutdown_event = threading.Event()

    def start(self):
        self._server.start()

    def wait_for_shutdown(self):
        self._shutdown_event.wait()

    def stop(self):
        self._server.stop(grace=None)

    def Shutdown(self, request, context):
        def shutdown_cb():
            self._shutdown_event.set()

        context.add_callback(shutdown_cb)
        return google.protobuf.empty_pb2.Empty()

if __name__ == "__main__":
    service = Service()
    service.start()
    service.wait_for_shutdown()
    service.stop()
