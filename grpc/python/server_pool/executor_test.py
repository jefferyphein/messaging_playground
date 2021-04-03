from concurrent.futures import as_completed
import grpc

import hello_pb2_grpc
import hello_pb2

import executor

if __name__ == '__main__':
    channels = [grpc.insecure_channel('localhost:%s'%port) for port in [50051,50052,50052,50054,50055]]
    pool = executor.grpcPoolExecutor(5, channels, hello_pb2_grpc.GreeterStub)

    Greeter = executor.ServiceWrapper(hello_pb2_grpc.GreeterStub)

    futs = []
    for i in range(100):
        req = hello_pb2.HelloRequest(name='name_%s'%i)
        f = pool.submit(Greeter.SayHello, req)
        futs.append(f)

    for f in as_completed(futs):
        print(f.result())
    pool.shutdown(close_channels=False)

    # test with statement

    with executor.grpcPoolExecutor(5, channels, hello_pb2_grpc.GreeterStub) as pool:
        f = pool.submit(Greeter.SayHello,
                        hello_pb2.HelloRequest(name='test')
                        )
        f.add_done_callback(lambda r: print("done"))
