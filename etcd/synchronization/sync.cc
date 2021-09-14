#include <cstring>
#include <iostream>
#include <exception>
#include <thread>
#include <absl/strings/str_format.h>
#include <absl/strings/numbers.h>
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
    , shutdown_protobuf_on_destroy(false)
{}

void sync_config_t::destroy() {
}

sync_t::sync_t(uint32_t whoami, uint32_t size)
    : whoami_(whoami)
    , size_(size)
    , state_(size, STATE_INVALID)
    , lease_(nullptr)
    , loop_(static_cast<uv_loop_t*>(malloc(sizeof(uv_loop_t))))
    , kv_stub_(nullptr)
    , started_(false)
    , global_state_(STATE_INVALID)
{}

void sync_t::destroy() {
    // Revoke the lease if necessary.
    if (lease_) {
        lease_->revoke();
    }

    if (watch_) {
        watch_->cancel();
    }

    // Stop the loop, then join the loop thread.
    uv_stop(loop_);
    if (loop_thread_) {
        loop_thread_->join();
    }

    // Shutdown the completion queue and join the thread.
    if (cq_thread_) {
        cq_.Shutdown();
        cq_thread_->join();
    }

    // Close and free the loop.
    uv_loop_close(loop_);
    free(loop_);

    // Shutdown the Protobuf library.
    if (conf_.shutdown_protobuf_on_destroy) {
        ::google::protobuf::ShutdownProtobufLibrary();
        std::cout << "SHUTDOWN" << std::endl;
    }

    conf_.destroy();
}

bool sync_t::start() {
    // Ensure that multiple threads cannot call start simultaneously.
    std::unique_lock<std::mutex> lck(started_mtx_);
    if (started_) return true;

    std::string key_start = ::absl::StrFormat("%s0", conf_.key_prefix);
    std::string key_end   = ::absl::StrFormat("%s%d", conf_.key_prefix, size_);

    ::etcdserverpb::WatchCreateRequest watch_create_request;
    watch_create_request.set_key(key_start);
    watch_create_request.set_range_end(key_end);
    watch_create_request.set_watch_id(0);

    kv_stub_ = ::etcdserverpb::KV::NewStub(::grpc::CreateChannel(conf_.remote_host, ::grpc::InsecureChannelCredentials()));
    uv_loop_init(loop_);

    lease_ = std::unique_ptr<Lease>(
        new Lease(conf_.remote_host,
                  loop_,
                  &cq_,
                  conf_.lease_ttl,
                  conf_.lease_heartbeat));

    watch_ = std::unique_ptr<Watch>(
        new Watch(conf_.remote_host,
                  watch_create_request,
                  &cq_,
                  [this](::etcdserverpb::WatchResponse& response){ this->watch_callback(response); }));

    // Start the loop thread *after* the lease and watch have been constructed.
    loop_thread_ = std::unique_ptr<std::thread>(new std::thread(&sync_t::run_loop_, this));
    cq_thread_ = std::unique_ptr<std::thread>(new std::thread(&sync_t::run_completion_queue_, this));

    set_state(STATE_INITIALIZED);
    started_ = true;
    return true;
}

void sync_t::run_loop_() {
    uv_run(loop_, UV_RUN_DEFAULT);
}

void sync_t::run_completion_queue_() {
    void *tag;
    bool ok = false;
    while (cq_.Next(&tag, &ok)) {
        static_cast<AsyncEtcdBase*>(tag)->proceed();
    }
}

static inline bool is_digits(const std::string& s) {
    return not s.empty() and s.find_first_not_of("0123456789") == std::string::npos;
}

static inline int update_state(const ::etcdserverpb::Event& event, size_t remote_id) {
    if (event.type() == ::etcdserverpb::Event_EventType_PUT) {
        if (is_digits(event.kv().value())) {
            return strtoul(event.kv().value().c_str(), NULL, 0);
        }
        else {
            return STATE_INVALID;
        }
    }
    else if (event.type() == ::etcdserverpb::Event_EventType_DELETE) {
        return STATE_INVALID;
    }
    return STATE_INVALID;
}

void sync_t::global_state_change_check() {
    int state = *std::min_element(state_.begin(), state_.end());
    std::unique_lock<std::mutex> lck(global_state_change_mtx_);
    if (state != global_state_.load()) {
        global_state_ = state;
        global_state_change_cv_.notify_all();
        std::cout << "global state change -> " << state << std::endl;
    }
}

void sync_t::watch_callback(::etcdserverpb::WatchResponse& response) {
    auto events = response.events();
    for (const ::etcdserverpb::Event& event : events) {
        if (event.kv().key().rfind(conf_.key_prefix, 0) == 0) {
            const std::string& basename = event.kv().key().substr(conf_.key_prefix.size());
            if (is_digits(basename)) {
                uint32_t remote_id = strtoul(basename.c_str(), NULL, 0);
                if (remote_id < size_) {
                    state_[remote_id] = update_state(event, remote_id);
                }
            }
        }
    }

    // Check to see if global state has changed once we're done processing all
    // of the Watch events.
    global_state_change_check();
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

    state_[whoami_] = state;

    return 0;
}

int sync_t::get_state(int remote_id) {
    if (remote_id == -1) {
        return *std::min_element(state_.begin(), state_.end());
    }
    return state_[remote_id];
}

void sync_t::wait_for_global_state(int state) {
    std::unique_lock<std::mutex> lck(global_state_change_mtx_);
    while (global_state_.load() < state) {
        global_state_change_cv_.wait(lck);
    }
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
    else if (strncmp(key, "shutdown-protobuf-on-destroy", 28) == 0) {
        S->conf_.shutdown_protobuf_on_destroy = (int64_t)strtol(value, NULL, 0) != 0;
    }
    else {
        sync_set_error(error, ::absl::StrFormat("Configuration failed: unrecognized key '%s'", key));
        return 1;
    }
    return 0;
}

int sync_start(sync_t *S,
                    char **error) {
    try {
        return S->start() ? 0 : 1;
    }
    catch (std::runtime_error& e) {
        sync_set_error(error, ::absl::StrFormat("Initialization failed: %s", e.what()));
        return 1;
    }
    catch (std::bad_alloc& e) {
        sync_set_error(error, ::absl::StrFormat("Initialization failed: %s", e.what()));
        return 1;
    }
}

int sync_set_state(sync_t *S,
                   int state,
                   char **error) {
    if (state < S->my_state()) {
        sync_set_error(error, ::absl::StrFormat("State can only increase: (desired) %d < %d (current)", state, S->my_state()));
        return 1;
    }
    else if (state == S->my_state()) {
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
    if (remote_id < -1 or remote_id >= static_cast<int>(S->size_)) {
        sync_set_error(error, ::absl::StrFormat("Remote ID must be in range [0,%d), or -1 to get global state", S->size_));
        return -2;
    }

    return S->get_state(remote_id);
}

int sync_wait_for_global_state(sync_t *S,
                               int desired_state,
                               char **error) {
    S->wait_for_global_state(desired_state);
    return 0;
}

int sync_destroy(sync_t *S,
                 char **error) {
    S->destroy();
    delete S;
    return 0;
}
