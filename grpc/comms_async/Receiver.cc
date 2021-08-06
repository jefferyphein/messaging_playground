#include <iostream>
#include <mutex>
#include <condition_variable>
#include <unistd.h>

#include "Receiver.h"

Receiver::Receiver(std::string address)
    : server_built_(false)
    , thread_(&Receiver::run_service, this, address)
    , counter_(0)
{}

void Receiver::run_service(std::string address) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    this->cq_ = builder.AddCompletionQueue();
    this->server_ = builder.BuildAndStart();
    this->server_built_ = true;

    if (this->server_ == nullptr) {
        std::cerr << "Unable to start server at " << address << std::endl;
        return;
    }

#if 0
    // Block until Shutdown() is called.
    this->server_->Wait();
#else
    new CallData(this, this->cq_.get());
    void *tag; // uniquely identifies a request.
    bool ok;
    while (true) {
        GPR_ASSERT(this->cq_->Next(&tag, &ok));
        ++counter_;
        if (not ok) { break; }
        static_cast<CallData*>(tag)->Proceed();
    }
#endif
}

void Receiver::shutdown() {
    // If the thread is not joinable, there's nothing to do here. The thread
    // can only be non-joinable if it failed to start, or it has already been
    // joined.
    if (this->thread_.joinable()) {
        std::mutex m;
        std::condition_variable cv;
        std::unique_lock<std::mutex> lck(m);

        // Wait until the server is built. This doesn't mean the server has
        // started, just that BuildAndStart() was called and returned. Wake up
        // once every millisecond to check predicate.
        //
        // This ensures the thread launching `run_server` has returned from
        // BuildAndStart() and set the value of `server_`. Without this barrier,
        // it is possible to deadlock when attempting to join the thread.
        while (!cv.wait_for(lck, std::chrono::milliseconds(1), [this]{ return this->server_built_; }));

        // Only call Shutdown() if the server is a valid pointer.
        if (this->server_ != nullptr) {
            this->server_->Shutdown();
            this->server_ = nullptr;
        }

        // Thread will join once the server shuts down gracefully.
        this->thread_.join();
    }
}

Receiver::~Receiver() {
    if (this->server_) {
        this->server_->Shutdown();
    }
    this->cq_->Shutdown();
}

Receiver::CallData::CallData(comms::Comms::AsyncService *service, grpc::ServerCompletionQueue *cq)
    : service_(service)
    , cq_(cq)
    , responder_(&ctx_)
    , status_(CREATE)
{
    this->Proceed();
}

void Receiver::CallData::Proceed() {
    if (this->status_ == CREATE) {
        this->status_ = PROCESS;
        this->service_->RequestSend(&this->ctx_, &this->request_, &this->responder_, this->cq_, this->cq_, this);
    }
    else if (this->status_ == PROCESS) {
        new CallData(this->service_, this->cq_);

        // TODO: Satisfy the request here.

        this->status_ = FINISH;
        this->responder_.Finish(this->response_, grpc::Status::OK, this);
    }
    else {
        delete this;
    }
}
