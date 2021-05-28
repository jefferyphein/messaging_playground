import grpc
import asyncio
import google.protobuf

import protos.shutdown_pb2
import protos.shutdown_pb2_grpc


async def main():
    channel = grpc.aio.insecure_channel("localhost:46307")
    stub = protos.shutdown_pb2_grpc.ShutdownStub(channel)
    await channel.channel_ready()

    request = google.protobuf.empty_pb2.Empty()
    response = await stub.Shutdown(request)

if __name__ == "__main__":
    asyncio.run(main())
