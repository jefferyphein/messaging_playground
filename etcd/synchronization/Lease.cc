#include <grpcpp/grpcpp.h>
#include <absl/strings/str_format.h>
#include <mutex>
#include <atomic>
#include <condition_variable>

extern "C" {
#include "sync.h"
}
#include "sync_impl.h"

Lease::Lease(std::string address, uv_loop_t *loop, ::grpc::CompletionQueue *cq, int64_t ttl, int64_t heartbeat)
        : id_(0)
        , cq_(cq)
        , heartbeat_(heartbeat)
        , stub_(::etcdserverpb::Lease::NewStub(::grpc::CreateChannel(address, ::grpc::InsecureChannelCredentials())))
        , next_state_(START)
{
    // Request a lease.
    ::etcdserverpb::LeaseGrantRequest request;
    request.set_id(0);
    request.set_ttl(ttl);
    ::etcdserverpb::LeaseGrantResponse response;
    ::grpc::ClientContext context;
    ::grpc::Status status = stub_->LeaseGrant(&context, request, &response);

    // If lease request failed, return error.
    if (!status.ok()) {
        throw std::runtime_error(::absl::StrFormat("RPC failed: %s", status.error_message()));
    }

    // Grab the lease ID from the response.
    id_ = response.id();

    // Create the asynchronous stream reader/writer for keep alive.
    stream_ = stub_->PrepareAsyncLeaseKeepAlive(&context_, cq_);
    proceed();

    // Create and start the keep alive timer.
    keep_alive_timer_.data = this;
    uv_timer_init(loop, &keep_alive_timer_);
    auto keep_alive_cb = [](uv_timer_t* handle) {
        static_cast<Lease*>(handle->data)->keep_alive();
    };
    uv_timer_start(&keep_alive_timer_, keep_alive_cb, 0, heartbeat*1000);
}

void Lease::proceed() {
    switch (next_state_) {
        case START:
            // Now that the stream is prepared, start it.
            next_state_ = START_DONE;
            stream_->StartCall(this);
            break;
        case START_DONE: {
            // Now that the stream is started, indicate ready to write.
            next_state_ = READY_TO_WRITE;
            writing_cv_.notify_all();
            break;
        }
        case WRITE_DONE:
            // Writing has completed, read response response.
            next_state_ = READ_DONE;
            stream_->Read(&response_, this);
            break;
        case READ_DONE:
            // Read has completed, indicate ready to write.
            next_state_ = READY_TO_WRITE;
            writing_cv_.notify_all();
            break;
        case WRITES_DONE_DONE:
            // WritesDone() has completed, place into invalid state.
            next_state_ = INVALID;
            break;
        case INVALID:
            throw std::runtime_error("Lease is in invalid state.");
            break;
    }
}

void Lease::keep_alive() {
    // Nothing to do if lease has been revoked.
    if (id_ == 0) return;

    if (next_state_ == READY_TO_WRITE) {
        // Send keep alive request, only if in READY_TO_WRITE state.
        ::etcdserverpb::LeaseKeepAliveRequest request;
        request.set_id(id_);

        next_state_ = WRITE_DONE;
        stream_->Write(request, this);
    }
}

void Lease::revoke() {
    // No lease to revoke, return immediately.
    if (id_ == 0) return;

    int64_t id = id_;

    // Close the stream and stop the timer.
    std::unique_lock<std::mutex> lck(writing_mtx_);
    if (next_state_ != READY_TO_WRITE) {
        writing_cv_.wait(lck);
    }
    next_state_ = WRITES_DONE_DONE;
    stream_->WritesDone(this);

    // Reset the lease id, stop the timer, and close its handle.
    id_ = 0;
    uv_timer_stop(&keep_alive_timer_);
    uv_close((uv_handle_t*)&keep_alive_timer_, [](uv_handle_t*){});

    // Revoke the lease.
    ::etcdserverpb::LeaseRevokeRequest request;
    request.set_id(id);
    ::etcdserverpb::LeaseRevokeResponse response;
    ::grpc::ClientContext context;
    ::grpc::Status status = stub_->LeaseRevoke(&context, request, &response);

    // NOTE: We don't really need to check for an error here, since the lease
    // will get automatically revoked on the remote end once the keep-alives
    // halt. Failure here just means that the lease revocation will just be
    // slightly delayed.
}
