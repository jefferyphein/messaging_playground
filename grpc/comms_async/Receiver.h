#pragma once

#include <thread>
#include <grpcpp/grpcpp.h>

#include "comms.grpc.pb.h"

class Receiver final : public comms::Comms::AsyncService {
public:
    Receiver() = delete;
    Receiver(std::string address);

    void run_service(std::string address);

    void shutdown();

    ~Receiver();

private:
    bool server_built_;
    std::thread thread_;
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<grpc::ServerCompletionQueue> cq_;
    std::atomic_int counter_;

    class CallData {
    public:
        CallData(comms::Comms::AsyncService *service,
                 grpc::ServerCompletionQueue *cq);

        void Proceed();

    private:
        // The means of communication with the gRPC runtime for an async server.
        comms::Comms::AsyncService *service_;

        // The producer-consumer queue for async server notifications.
        grpc::ServerCompletionQueue *cq_;

        // Context for the RPC, allowing to tweak aspects of it such as the use
        // of compression, authentication, as well as to send metadata back to the
        // client.
        grpc::ServerContext ctx_;

        // What we get from the client.
        comms::Packets request_;

        // What we send back to the client.
        comms::PacketResponse response_;

        // The means to get back to the client.
        grpc::ServerAsyncResponseWriter<comms::PacketResponse> responder_;

        // The state machine.
        enum CallStatus { CREATE, PROCESS, FINISH };
        CallStatus status_; // The current serving state.
    };
};
