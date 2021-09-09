#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <thread>
#include <uv.h>

#include "etcd.pb.h"
#include "etcd.grpc.pb.h"

class Lease;
using LeaseStream = ::grpc::ClientReaderWriter<::etcdserverpb::LeaseKeepAliveRequest, ::etcdserverpb::LeaseKeepAliveResponse>;

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
    void initialize();
    void destroy();
    void run_loop_();
    int set_state(uint32_t state);
    int get_state(int remote_id);

    uint32_t whoami_;
    uint32_t size_;
    uint32_t state_;
    sync_config_t conf_;
    std::unique_ptr<Lease> lease_;
    std::unique_ptr<std::thread> loop_thread_;
    uv_loop_t *loop_;
    std::unique_ptr<::etcdserverpb::KV::Stub> kv_stub_;
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
    std::unique_ptr<LeaseStream> lease_stream_;

    std::mutex writing_mtx_;

    void keep_alive();
};
