#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
extern "C" {
#include "sync.h"
}
#include "sync_impl.h"

Watch::Watch(std::string address,
             ::etcdserverpb::WatchCreateRequest& create_request,
             ::grpc::CompletionQueue *cq,
             watch_function watch_callback)
    : cq_(cq)
    , stub_(::etcdserverpb::Watch::NewStub(::grpc::CreateChannel(address, ::grpc::InsecureChannelCredentials())))
    , watch_id_(-1)
    , next_state_(START)
    , create_request_(new ::etcdserverpb::WatchCreateRequest(create_request))
    , watch_callback_(watch_callback)
    , canceling_(false)
{
    stream_ = stub_->PrepareAsyncWatch(&context_, cq_);
    proceed();
}

void Watch::proceed() {
    switch (next_state_) {
        case START:
            // Now that the stream is prepared, start it.
            next_state_ = START_DONE;
            stream_->StartCall(this);
            break;
        case START_DONE: {
            // Now that the stream is started, create a watch request.
            ::etcdserverpb::WatchRequest request;
            request.set_allocated_create_request(create_request_);
            next_state_ = CREATE;
            stream_->Write(request, this);
            break;
        }
        case CREATE:
            // The watch request was written, wait for the server response.
            next_state_ = CREATE_DONE;
            stream_->Read(&response_, this);
            break;
        case CREATE_DONE:
            // Get the watch ID from the server response, then start listening
            // for updates from the stream.
            watch_id_ = response_.watch_id();
            next_state_ = UPDATE;
            stream_->Read(&response_, this);
            break;
        case UPDATE: {
            if (canceling_ and watch_id_ != -1) {
                // Write a request to cancel the watch.
                ::etcdserverpb::WatchRequest request;
                auto *cancel_request = request.mutable_cancel_request();
                cancel_request->set_watch_id(watch_id_);
                next_state_ = CANCEL_DONE;
                stream_->Write(request, this);
            }
            else {
                // Process the request.
                watch_callback_(response_);
                stream_->Read(&response_, this);
            }
            break;
        }
        case CANCEL_DONE:
            // Process requests until we get confirmation that the remote end
            // canceled the watch.
            if (response_.canceled()) {
                watch_id_ = -1;
                next_state_ = WRITES_DONE_DONE;
                stream_->WritesDone(this);
            }
            break;
        case WRITES_DONE_DONE:
            // Stream acknowledged that no more writes are possible.
            next_state_ = INVALID;
            //cq_->Shutdown();
            break;
    }
}

void Watch::cancel() {
    if (watch_id_ == -1) return;

    // Place an alarm into the completion queue indicating our intention to
    // cancel the watch.
    canceling_ = true;
    ::grpc::Alarm alarm;
    alarm.Set(cq_, gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME), this);
}
