#pragma once

#include <thread>
#include <grpcpp/grpcpp.h>

#include "comms.grpc.pb.h"

class Receiver final : public comms::Comms::Service {
public:
    Receiver() = delete;
    Receiver(std::string address);

    void run_service(std::string address);

    grpc::Status Send(grpc::ServerContext *context, const comms::Packet *request, comms::PacketResponse *response) override;

    void stop();

    ~Receiver();

private:
    bool ready_;
    std::thread thread_;
    std::unique_ptr<grpc::Server> server_;
};
