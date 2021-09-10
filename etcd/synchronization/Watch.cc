#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
extern "C" {
#include "sync.h"
}
#include "sync_impl.h"

Watch::Watch(std::string address,
             ::etcdserverpb::WatchCreateRequest& create_request,
             watch_function watch_callback)
    : stub_(::etcdserverpb::Watch::NewStub(::grpc::CreateChannel(address, ::grpc::InsecureChannelCredentials())))
    , watch_id_(-1)
    , status_(START)
    , create_request_(new ::etcdserverpb::WatchCreateRequest(create_request))
    , canceling_(false)
    , watch_callback_(watch_callback)
{
    stream_ = stub_->PrepareAsyncWatch(&context_, &cq_);
    thread_ = std::unique_ptr<std::thread>(new std::thread(&Watch::watch_thread, this, std::ref(create_request)));
}

void Watch::watch_thread(::etcdserverpb::WatchCreateRequest& create_request) {
    proceed();

    void *tag;
    bool ok = false;
    while (cq_.Next(&tag, &ok)) {
        static_cast<Watch*>(tag)->proceed();
    }

    watch_id_ = -1;
}

Watch::~Watch() {
    if (thread_) {
        cancel();
        thread_->join();
    }
}

void Watch::proceed() {
    switch (status_) {
        case START:
            // Now that the stream is prepared, start it.
            status_ = START_DONE;
            stream_->StartCall(this);
            break;
        case START_DONE: {
            // Now that the stream is started, create a watch request.
            ::etcdserverpb::WatchRequest request;
            request.set_allocated_create_request(create_request_);
            status_ = CREATE;
            stream_->Write(request, this);
            break;
        }
        case CREATE:
            // The watch request was written, wait for the server response.
            status_ = CREATE_DONE;
            stream_->Read(&response_, this);
            break;
        case CREATE_DONE:
            // Get the watch ID from the server response, then start listening
            // for updates from the stream.
            watch_id_ = response_.watch_id();
            status_ = UPDATE;
            stream_->Read(&response_, this);
            break;
        case UPDATE: {
            if (canceling_) {
                // If cancel() has been called, request to cancel the watch.
                ::etcdserverpb::WatchRequest request;
                auto *cancel_request = request.mutable_cancel_request();
                cancel_request->set_watch_id(watch_id_);
                status_ = CANCEL;
                stream_->Write(request, this);
                break;
            }

            // Process this response.
            watch_callback_(response_);
            stream_->Read(&response_, this);
            break;
        }
        case CANCEL: {
            // The cancel request was written, continue processing responses
            // until we receive cancelation confirmation.
            if (response_.canceled()) {
                cq_.Shutdown();
            }
            break;
        }
    }
}

void Watch::cancel() {
    if (watch_id_ == -1) return;

    // Place an alarm into the completion queue indicating our intention to
    // cancel the watch.
    canceling_ = true;
    ::grpc::Alarm alarm;
    alarm.Set(&cq_, gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME), this);
}
