#pragma once

#include <thread>
#include <grpcpp/grpcpp.h>

#include "comms.grpc.pb.h"

class Receiver final : public comms::Comms::Service {
public:
    Receiver() = delete;
    Receiver(std::string address);

    void run_service(std::string address);

    grpc::Status Send(grpc::ServerContext *context, const comms::Packets *request, comms::PacketResponse *response) override;

    void shutdown();

    ~Receiver();

private:
    bool server_built_;
    std::thread thread_;
    std::unique_ptr<grpc::Server> server_;
};
