#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <uv.h>

#include "etcd.pb.h"
#include "etcd.grpc.pb.h"

#define STATE_INVALID (-1)
#define STATE_INITIALIZED (0)

#define CANCEL_TAG (1)

class Lease;
class Watch;

using LeaseStream = ::grpc::ClientReaderWriter<::etcdserverpb::LeaseKeepAliveRequest, ::etcdserverpb::LeaseKeepAliveResponse>;
using WatchStream = ::grpc::ClientAsyncReaderWriter<::etcdserverpb::WatchRequest, ::etcdserverpb::WatchResponse>;
using watch_function = std::function<void(::etcdserverpb::WatchResponse&)>;

void sync_set_error(char **error, std::string str);

typedef struct sync_config_t {
    sync_config_t();
    void destroy();

    std::string remote_host;
    int64_t lease_ttl;
    int64_t lease_heartbeat;
    std::string key_prefix;
} sync_config_t;

typedef struct sync_t {
    sync_t(uint32_t whoami, uint32_t size);
    bool start();
    void destroy();
    void run_loop_();
    int set_state(uint32_t state);
    int get_state(int remote_id);
    inline int my_state() const {
        return state_[whoami_];
    }
    void watch_callback(::etcdserverpb::WatchResponse& response);

    uint32_t whoami_;
    uint32_t size_;
    std::vector<int> state_;
    sync_config_t conf_;
    std::unique_ptr<Lease> lease_;
    std::unique_ptr<Watch> watch_;
    std::unique_ptr<std::thread> loop_thread_;
    uv_loop_t *loop_;
    std::unique_ptr<::etcdserverpb::KV::Stub> kv_stub_;
    std::atomic_bool started_;
    std::mutex started_mtx_;
} sync_t;

class Lease {
public:
    Lease(std::string address, uv_loop_t *loop, int64_t ttl, int64_t heartbeat);
    ~Lease();
    void revoke();
    inline int64_t id() const {
        return id_;
    }

private:
    int64_t id_;
    std::unique_ptr<::etcdserverpb::Lease::Stub> stub_;
    uv_timer_t keep_alive_timer_;
    std::unique_ptr<::grpc::ClientContext> context_;
    std::unique_ptr<LeaseStream> stream_;

    std::mutex writing_mtx_;

    void keep_alive();
};

class Watch {
public:
    Watch(std::string address,
          ::etcdserverpb::WatchCreateRequest& create_request,
          watch_function watch_callback);
    void proceed();
    void cancel();
    ~Watch();

private:
    void watch_thread(::etcdserverpb::WatchCreateRequest& create_request);
    void watch_callback(::etcdserverpb::WatchResponse& response);

    enum StreamState {
        START,
        START_DONE,
        CREATE,
        CREATE_DONE,
        UPDATE,
        CANCELING,
        CANCEL,
        CANCEL_DONE,
        WRITES_DONE_DONE,
        INVALID
    };

    ::grpc::CompletionQueue cq_;
    ::grpc::ClientContext context_;
    ::etcdserverpb::WatchResponse response_;
    ::grpc::Status status_;
    std::unique_ptr<::etcdserverpb::Watch::Stub> stub_;
    int64_t watch_id_;
    StreamState next_state_;
    ::etcdserverpb::WatchCreateRequest *create_request_;
    std::unique_ptr<WatchStream> stream_;
    std::unique_ptr<std::thread> thread_;
    watch_function watch_callback_;
};
