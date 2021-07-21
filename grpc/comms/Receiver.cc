#include <iostream>
#include <mutex>
#include <condition_variable>
#include <unistd.h>

#include "Receiver.h"

Receiver::Receiver(std::string address) :
        thread_(&Receiver::run_service, this, address),
        server_built_(false) {
}

void Receiver::run_service(std::string address) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    this->server_ = builder.BuildAndStart();
    this->server_built_ = true;

    if (this->server_ == nullptr) {
        std::cerr << "Unable to start server at " << address << std::endl;
        return;
    }

    this->server_->Wait();
}

grpc::Status Receiver::Send(grpc::ServerContext *context, const comms::Packets *request, comms::PacketResponse *response) {
    return grpc::Status::OK;
}

void Receiver::stop() {
    // If the thread is not joinable, there's nothing to do here. The thread
    // can only be non-joinable if it failed to start, or it has already been
    // joined.
    if (this->thread_.joinable()) {
        std::mutex m;
        std::condition_variable cv;
        std::unique_lock<std::mutex> lck(m);

        // Wait until the server is built. This doesn't mean the server has
        // started, just that BuildAndStart() was called and returned.
        while (!cv.wait_for(lck, std::chrono::milliseconds(10), [this]{ return this->server_built_; }));

        // Only call Shutdown() if the server is a valid pointer.
        if (this->server_ != nullptr) {
            this->server_->Shutdown();
            this->server_ = nullptr;
        }

        // Join the thread once Wait() returns.
        this->thread_.join();
    }
}

Receiver::~Receiver() {
    this->stop();
}
