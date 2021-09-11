#include <grpcpp/grpcpp.h>
#include <absl/strings/str_format.h>
#include <mutex>
#include <atomic>
#include <condition_variable>

extern "C" {
#include "sync.h"
}
#include "sync_impl.h"

Lease::Lease(std::string address, uv_loop_t *loop, int64_t ttl, int64_t heartbeat)
        : id_(0)
        , stub_(::etcdserverpb::Lease::NewStub(::grpc::CreateChannel(address, ::grpc::InsecureChannelCredentials())))
        , context_(new ::grpc::ClientContext)
{
    // If heartbeat is >= ttl, set heartbeat to half the ttl.
    if (heartbeat >= ttl) {
        heartbeat = std::max(1ll, heartbeat/2ll);
    }

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

    // TODO: This should eventually be made into an async stream.
    // Create the stream reader/writer for keep alive.
    stream_ = stub_->LeaseKeepAlive(context_.get());

    // Create and start the keep alive timer.
    keep_alive_timer_.data = this;
    uv_timer_init(loop, &keep_alive_timer_);
    auto keep_alive_cb = [](uv_timer_t* handle) {
        static_cast<Lease*>(handle->data)->keep_alive();
    };
    uv_timer_start(&keep_alive_timer_, keep_alive_cb, 0, heartbeat*1000);
}

Lease::~Lease() {
    // Revoke an outstanding lease, if revoke() wasn't called directly.
    if (id_) {
        revoke();
    }
}

void Lease::keep_alive() {
    std::unique_lock<std::mutex> lck(writing_mtx_);

    // Nothing to do if lease has been revoked.
    if (id_ == 0) return;

    // Send keep alive request.
    ::etcdserverpb::LeaseKeepAliveRequest request;
    request.set_id(id_);

    bool ok = stream_->Write(request);

    if (not ok) {
        // Write failed, do something.
    }

    // Read keep alive response.
    ::etcdserverpb::LeaseKeepAliveResponse response;
    ok = stream_->Read(&response);
    if (not ok) {
        // Read failed, do something.
    }
}

void Lease::revoke() {
    // No lease to revoke, return immediately.
    if (id_ == 0) return;

    int64_t id = id_;

    // Close the stream and stop the timer.
    {
        std::unique_lock<std::mutex> lck(writing_mtx_);
        stream_->WritesDone();
        id_ = 0;
    }
    uv_timer_stop(&keep_alive_timer_);

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
