extern "C" {
#include "comms.h"
}
#include "comms_impl.h"

comms_receiver_t::comms_receiver_t(std::vector<std::shared_ptr<comms_reader_t>>& readers)
        : started_(false)
        , shutting_down_(false)
        , shutdown_(false)
        , readers_(readers)
{}

void comms_receiver_t::start(std::string address) {
    thread_ = std::make_shared<std::thread>(&comms_receiver_t::run, this, address);
}

void comms_receiver_t::run(std::string address) {
    for (auto reader : readers_) {
        reader->wait_for_start();
    }

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    server_ = builder.BuildAndStart();

    if (server_ == nullptr) {
        //std::stringstream ss;
        //ss << "Unable to start server at " << address_;
        //comms_set_error(error, ss.str().c_str());
        return;
    }

    {
        std::unique_lock<std::mutex> lck(started_mtx_);
        started_ = true;
        started_cv_.notify_all();
    }

    server_->Wait();

    // Acquire shutdown lock and notify shutdown.
    std::unique_lock<std::mutex> lck(shutdown_mtx_);
    shutdown_ = true;
    shutdown_cv_.notify_all();
}

void comms_receiver_t::wait_for_start() {
    if (shutdown_) return;
    if (shutting_down_) return;

    std::unique_lock<std::mutex> lck(started_mtx_);
    if (started_) return;
    started_cv_.wait(lck);
}

void comms_receiver_t::shutdown() {
    if (shutdown_) return;
    if (shutting_down_) return;

    server_->Shutdown();
    shutting_down_ = true;
}

void comms_receiver_t::wait_for_shutdown() {
    std::unique_lock<std::mutex> lck(shutdown_mtx_);
    shutdown_cv_.wait(lck);
    thread_->join();
}

grpc::Status CommsServiceImpl::Send(grpc::ServerContext *context,
                                    const comms::Packets *request,
                                    comms::PacketResponse *response) {
    // TODO: Forward incoming request to a reader.
    return grpc::Status::OK;
}
