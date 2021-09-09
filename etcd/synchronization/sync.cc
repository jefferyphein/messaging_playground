#include <cstring>
#include <iostream>
#include <exception>
#include <thread>
#include <absl/strings/str_format.h>
#include <grpcpp/grpcpp.h>

extern "C" {
#include "sync.h"
}
#include "sync_impl.h"

void sync_set_error(char **error, std::string str) {
    size_t len = str.size();
    error[0] = (char*)calloc(len+1, sizeof(char));
    strncpy(error[0], str.c_str(), len+1);
}

sync_config_t::sync_config_t()
    : lease_ttl(15)
    , lease_heartbeat(5)
    , key_prefix("/sync/state/")
{}

void sync_config_t::destroy() {
}

sync_t::sync_t(uint32_t whoami, uint32_t size)
    : whoami_(whoami)
    , size_(size)
    , state_(0)
    , conf_()
    , lease_(nullptr)
    , loop_(static_cast<uv_loop_t*>(malloc(sizeof(uv_loop_t))))
    , kv_stub_(nullptr)
{}

void sync_t::destroy() {
    // Revoke the lease if necessary.
    if (lease_) {
        lease_->revoke();
    }

    // Stop the loop, then join the loop thread.
    uv_stop(loop_);
    if (loop_thread_) {
        loop_thread_->join();
    }

    // Close and free the loop.
    uv_loop_close(loop_);
    free(loop_);

    conf_.destroy();
}

void sync_t::initialize() {
    kv_stub_ = ::etcdserverpb::KV::NewStub(::grpc::CreateChannel(conf_.remote_host, ::grpc::InsecureChannelCredentials()));
    uv_loop_init(loop_);
    lease_ = std::unique_ptr<Lease>(new Lease(conf_.remote_host, loop_, conf_.lease_ttl, conf_.lease_heartbeat));
    loop_thread_ = std::unique_ptr<std::thread>(new std::thread(&sync_t::run_loop_, this));

    set_state(0);
}

void sync_t::run_loop_() {
    uv_run(loop_, UV_RUN_DEFAULT);
}

int sync_t::set_state(uint32_t state) {
    ::etcdserverpb::PutRequest request;
    request.set_key(::absl::StrFormat("%s%d", conf_.key_prefix, whoami_));
    request.set_value(::absl::StrFormat("%d", state));
    request.set_lease(lease_->id());

    ::etcdserverpb::PutResponse response;
    ::grpc::ClientContext context;
    ::grpc::Status status = kv_stub_->Put(&context, request, &response);

    if (!status.ok()) {
        throw std::runtime_error(::absl::StrFormat("RPC failed: %s", status.error_message()));
    }

    state_ = state;

    return 0;
}

int sync_t::get_state(int remote_id) {
    // TODO: Under construction.
    return state_;
}

int sync_create(sync_t **S,
                uint32_t local_id,
                uint32_t universe_size,
                char **error) {
    try {
        S[0] = new sync_t(local_id, universe_size);
        return 0;
    }
    catch (std::bad_alloc& e) {
        sync_set_error(error, ::absl::StrFormat("Unable to allocate memory."));
        return 1;
    }
}

int sync_configure(sync_t *S,
                   const char *key,
                   const char *value,
                   char **error) {
    int len = strlen(value);
    if (strncmp(key, "remote-host", 11) == 0) {
        S->conf_.remote_host = value;
    }
    else if (strncmp(key, "lease-ttl", 9) == 0) {
        S->conf_.lease_ttl = (int64_t)strtol(value, NULL, 0);
    }
    else if (strncmp(key, "lease-heartbeat", 15) == 0) {
        S->conf_.lease_heartbeat = (int64_t)strtol(value, NULL, 0);
    }
    else if (strncmp(key, "key-prefix", 10) == 0) {
        S->conf_.key_prefix = value;
    }
    return 0;
}

int sync_initialize(sync_t *S,
                    char **error) {
    try {
        S->initialize();
    }
    catch (std::runtime_error& e) {
        sync_set_error(error, ::absl::StrFormat("Initialization failed: %s", e.what()));
        return 1;
    }
    return 0;
}

int sync_set_state(sync_t *S,
                   uint32_t state,
                   char **error) {
    if (state < S->state_) {
        sync_set_error(error, ::absl::StrFormat("State can only increase: (desired) %d < %d (current)", state, S->state_));
        return 1;
    }
    else if (state == S->state_) {
        // No change, do nothing.
        return 0;
    }

    try {
        return S->set_state(state);
    }
    catch (std::runtime_error& e) {
        sync_set_error(error, ::absl::StrFormat("Set state failed: %s", e.what()));
        return 1;
    }
}

int sync_get_state(sync_t *S,
                        int remote_id,
                        char **error) {
    if (remote_id < -1 or remote_id >= S->size_) {
        sync_set_error(error, ::absl::StrFormat("Remote ID must be in range [0,%d), or -1 to get global state", S->size_));
        return 1;
    }

    return S->get_state(remote_id);
}

int sync_destroy(sync_t *S,
                 char **error) {
    S->destroy();
    delete S;
    return 0;
}
