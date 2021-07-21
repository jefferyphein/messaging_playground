#include <iostream>
#include <unistd.h>

#include "Receiver.h"

Receiver::Receiver(std::string address) :
        thread_(&Receiver::run_service, this, address) {
}

void Receiver::run_service(std::string address) {
    sleep(1);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(this);

    this->server_ = builder.BuildAndStart();
    if (this->server_ == nullptr) {
        std::cerr << "Unable to start server at " << address << std::endl;
        return;
    }
    std::cout << "Server listening on " << address << std::endl;

    this->server_->Wait();
}

grpc::Status Receiver::Send(grpc::ServerContext *context, const comms::Packet *request, comms::PacketResponse *response) {
    std::cout << "Send" << std::endl;
    return grpc::Status::OK;
}

void Receiver::stop() {
    if (this->server_ != nullptr) {
        std::cout << "Shutting down..." << std::endl;
        this->server_->Shutdown();
        std::cout << "Shutdown." << std::endl;
    }

    if (this->thread_.joinable()) {
        std::cout << "Joining thread..." << std::endl;
        this->thread_.join();
        std::cout << "Thread joined." << std::endl;
    }
}

Receiver::~Receiver() {
    this->stop();
}
