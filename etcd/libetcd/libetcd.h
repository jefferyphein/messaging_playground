#pragma once

#include <string>
#include <future>
#include <exception>
#include <stdexcept>
#include <grpcpp/grpcpp.h>

#include <iostream>

#include "etcd.grpc.pb.h"
#include "etcd.pb.h"

namespace libetcd {

using AsyncRangeResponseReader = ::grpc::ClientAsyncResponseReader<::etcdserverpb::RangeResponse>;

class AsyncEtcdBase {
public:
    AsyncEtcdBase() = default;
    virtual void proceed() = 0;
};

class Value {
public:
    Value()
        : key_("")
        , create_revision_(0)
        , mod_revision_(0)
        , version_(0)
        , value_("")
        , lease_(0)
    {}

    Value(const ::etcdserverpb::KeyValue& kv);

    const std::string& key() const { return key_; }
    std::string& key() { return key_; }
    int64_t created() const { return create_revision_; }
    int64_t modified() const { return mod_revision_; }
    int64_t version() const { return version_; }
    const std::string& value() const { return value_; }
    std::string& value() { return value_; }
    int64_t lease() const { return lease_; }

private:
    std::string key_;
    int64_t create_revision_;
    int64_t mod_revision_;
    int64_t version_;
    std::string value_;
    int64_t lease_;
};

typedef std::vector<Value> Values;

class Response {
public:
    Response() = delete;
    Response(::grpc::Status& status);
    Response(::grpc::Status& status, ::etcdserverpb::RangeResponse& response);

    bool ok() const;
    size_t size() const { return values_.size(); }
    const Values& values() const { return values_; }
    Values& values() { return values_; }
    const Value& value() const { return value_; }
    Value& value() { return value_; }

private:
    ::grpc::Status status_;
    Values values_;
    Value value_;
};

class Future {
public:
    Future();
    Response get();
    void set_value(::grpc::Status& status);
    void set_value(::grpc::Status& status,
                   ::etcdserverpb::RangeResponse& response);

private:
    std::shared_ptr<std::promise<Response>> prom_;
};

class Client {
public:
    Client(std::string address);
    ~Client();

    Future get(std::string key,
               std::string range_end = "");

private:
    std::string address_;
    std::unique_ptr<::etcdserverpb::KV::Stub> kv_stub_;
    std::unique_ptr<std::thread> cq_thread_;
    ::grpc::CompletionQueue cq_;

    void run_completion_queue_();

    class Request: public AsyncEtcdBase {
    public:
        Request(const ::etcdserverpb::RangeRequest& request,
                Future fut,
                ::etcdserverpb::KV::Stub *stub,
                ::grpc::CompletionQueue *cq);

        void proceed() override;

    private:
        Future fut_;
        ::etcdserverpb::RangeResponse response_;
        ::grpc::ClientContext context_;
        ::grpc::Status status_;
    };
};

class rpc_error: public std::runtime_error {
public:
    rpc_error(const char *msg)
        : std::runtime_error(msg)
    {}
};

}
