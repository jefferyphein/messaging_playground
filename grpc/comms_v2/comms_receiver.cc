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
#if COMMS_ASYNC_SERVICE
    cq_ = builder.AddCompletionQueue();
#endif
    server_ = builder.BuildAndStart();

    if (server_ == nullptr) {
        goto shutdown;
    }

    {
        std::unique_lock<std::mutex> lck(started_mtx_);
        started_ = true;
        started_cv_.notify_all();
    }

#if COMMS_ASYNC_SERVICE
    new CallData(&service_, cq_.get());
    void *tag;
    bool ok;
    while (true) {
        bool got_event = cq_->Next(&tag, &ok);

        // If `got_event` is false, the queue is fully drained and shut down.
        if (not got_event) {
            break;
        }

        // Proceed on the tag (pointer to a CallData object) only if we
        // successfully read an event.
        if (ok) {
            static_cast<CallData*>(tag)->Proceed();
        }
        else {
            // Server was shut down before this call was matched to an
            // incoming RPC. You win some, you lose some.
            delete static_cast<CallData*>(tag);
        }
    }
#endif

#if not COMMS_ASYNC_SERVICE
    server_->Wait();
#endif

shutdown:
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

    shutting_down_ = true;
    server_->Shutdown();
#if COMMS_ASYNC_SERVICE
    // The completion queue must always be shut down *after* the server.
    // https://grpc.io/docs/languages/cpp/async/#shutting-down-the-server
    cq_->Shutdown();
#endif
}

void comms_receiver_t::wait_for_shutdown() {
    std::unique_lock<std::mutex> lck(shutdown_mtx_);
    shutdown_cv_.wait(lck);
    thread_->join();
}

#if COMMS_ASYNC_SERVICE
comms_receiver_t::CallData::CallData(comms::Comms::AsyncService *service,
                                     grpc::ServerCompletionQueue *cq)
        : service_(service)
        , cq_(cq)
        , responder_(&ctx_)
        , status_(CREATE) {
    Proceed();
}

void comms_receiver_t::CallData::Proceed() {
    if (status_ == CREATE) {
        status_ = PROCESS;
        service_->RequestSend(&ctx_, &request_, &responder_, cq_, cq_, this);
    }
    else if (status_ == PROCESS) {
        new CallData(service_, cq_);
        status_ = FINISH;
        responder_.Finish(response_, grpc::Status::OK, this);
    }
    else {
        GPR_ASSERT( status_ == FINISH );
        delete this;
    }
}
#endif

#if not COMMS_ASYNC_SERVICE
grpc::Status CommsSyncServiceImpl::Send(grpc::ServerContext *context,
                                        const comms::Packets *request,
                                        comms::PacketResponse *response) {
    // TODO: Forward incoming request to a reader.
    return grpc::Status::OK;
}
#endif
