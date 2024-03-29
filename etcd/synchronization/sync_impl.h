#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <uv.h>

#include "etcd.pb.h"
#include "etcd.grpc.pb.h"

#define STATE_INVALID (-1)
#define STATE_INITIALIZED (0)

class Lease;
class Watch;

using LeaseKeepAliveStream = ::grpc::ClientAsyncReaderWriter<::etcdserverpb::LeaseKeepAliveRequest, ::etcdserverpb::LeaseKeepAliveResponse>;
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
    bool shutdown_protobuf_on_destroy;
} sync_config_t;

typedef struct sync_t {
    sync_t(uint32_t whoami, uint32_t size);
    bool start();
    void destroy();
    void run_loop_();
    void run_completion_queue_();
    int set_state(uint32_t state);
    int get_state(int remote_id);
    inline int my_state() const {
        return state_[whoami_];
    }
    void global_state_change_check();
    void watch_callback(::etcdserverpb::WatchResponse& response);
    void wait_for_global_state(int state);

    uint32_t whoami_;
    uint32_t size_;
    std::vector<int> state_;
    sync_config_t conf_;
    std::unique_ptr<Lease> lease_;
    std::unique_ptr<Watch> watch_;
    std::unique_ptr<std::thread> loop_thread_;
    std::unique_ptr<std::thread> cq_thread_;
    uv_loop_t *loop_;
    ::grpc::CompletionQueue cq_;
    std::unique_ptr<::etcdserverpb::KV::Stub> kv_stub_;
    std::atomic_bool started_;
    std::mutex started_mtx_;

    std::atomic<int> global_state_;
    std::condition_variable global_state_change_cv_;
    std::mutex global_state_change_mtx_;
} sync_t;

class AsyncEtcdBase {
public:
    AsyncEtcdBase() = default;

    virtual void proceed() = 0;
};

class Lease: public AsyncEtcdBase {
public:
    Lease(std::string address, uv_loop_t *loop, ::grpc::CompletionQueue *cq, int64_t ttl, int64_t heartbeat);
    void revoke();
    void proceed() override;
    inline int64_t id() const {
        return id_;
    }

private:
    void keep_alive();

    enum LeaseKeepAliveStreamState {
        START,
        START_DONE,
        READY_TO_WRITE,
        WRITE_DONE,
        READ_DONE,
        WRITES_DONE_DONE,
        INVALID
    };

    int64_t id_;
    ::grpc::CompletionQueue *cq_;
    int64_t heartbeat_;
    std::unique_ptr<::etcdserverpb::Lease::Stub> stub_;
    uv_timer_t keep_alive_timer_;
    ::grpc::ClientContext context_;
    std::unique_ptr<LeaseKeepAliveStream> stream_;
    std::mutex writing_mtx_;
    std::condition_variable writing_cv_;
    LeaseKeepAliveStreamState next_state_;
    ::etcdserverpb::LeaseKeepAliveResponse response_;
};

class Watch: public AsyncEtcdBase {
public:
    Watch(std::string address,
          ::etcdserverpb::WatchCreateRequest& create_request,
          ::grpc::CompletionQueue *cq,
          watch_function watch_callback);
    void proceed() override;
    void cancel();

private:
    void watch_thread(::etcdserverpb::WatchCreateRequest& create_request);
    void watch_callback(::etcdserverpb::WatchResponse& response);

    enum WatchStreamState {
        START,
        START_DONE,
        CREATE,
        CREATE_DONE,
        UPDATE,
        CANCEL_DONE,
        WRITES_DONE_DONE,
        INVALID
    };

    ::grpc::CompletionQueue *cq_;
    ::grpc::ClientContext context_;
    ::etcdserverpb::WatchResponse response_;
    ::grpc::Status status_;
    std::unique_ptr<::etcdserverpb::Watch::Stub> stub_;
    int64_t watch_id_;
    WatchStreamState next_state_;
    ::etcdserverpb::WatchCreateRequest *create_request_;
    std::unique_ptr<WatchStream> stream_;
    watch_function watch_callback_;

    std::atomic_bool canceling_;
};
