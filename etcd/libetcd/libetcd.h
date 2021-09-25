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
        : key_()
        , create_revision_(0)
        , mod_revision_(0)
        , version_(0)
        , value_()
        , lease_(0)
        , valid_(false)
    {}

    Value(const ::etcdserverpb::KeyValue& kv);

    const std::string& key() const { return key_; }
    std::string& key() { return key_; }
    int64_t created() const { return create_revision_; }
    int64_t modified() const { return mod_revision_; }
    int64_t version() const { return version_; }
    const std::string& string() const { return value_; }
    std::string& string() { return value_; }
    int64_t lease() const { return lease_; }
    bool is_valid() const { return valid_; }

private:
    std::string key_;
    int64_t create_revision_;
    int64_t mod_revision_;
    int64_t version_;
    std::string value_;
    int64_t lease_;
    bool valid_;
};

typedef std::vector<Value> Values;

class Response {
public:
    Response() = delete;
    Response(::grpc::Status& status);
    Response(::grpc::Status& status, ::etcdserverpb::RangeResponse& response, bool is_range = false);
    Response(::grpc::Status& status, ::etcdserverpb::PutResponse& response);

    size_t size() const { return values_.size(); }
    const Values& values() const { return values_; }
    Values& values() { return values_; }
    const Value& value(size_t index = 0) const;
    Value& value(size_t index = 0);

    const Value& prev_value() const { return prev_value_; }
    Value& prev_value() { return prev_value_; }

    bool ok() const;
    std::string error_message() const { return error_message_; }
    int error_code() const { return error_code_; }
    bool network_unavailable() const;

private:
    Values values_;
    Value prev_value_;
    std::string error_message_;
    int error_code_;
    bool is_range_;
};

class Future {
public:
    Future(bool is_range = false);
    Response get();
    void set_value(::grpc::Status& status);
    void set_value(::grpc::Status& status,
                   ::etcdserverpb::RangeResponse& response);
    void set_value(::grpc::Status& status,
                   ::etcdserverpb::PutResponse& response);

private:
    std::shared_ptr<std::promise<Response>> prom_;
    bool is_range_;
};

class Client {
public:
    Client(std::string address);
    ~Client();

    Future get(std::string key,
               std::string range_end = "");

    Future set(std::string key,
               std::string value,
               bool prev_kv = true);

private:
    std::string address_;
    std::unique_ptr<::etcdserverpb::KV::Stub> kv_stub_;
    std::unique_ptr<std::thread> cq_thread_;
    ::grpc::CompletionQueue cq_;

    void run_completion_queue_();

    class RangeRequest: public AsyncEtcdBase {
    public:
        RangeRequest(const ::etcdserverpb::RangeRequest& request,
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

    class PutRequest: public AsyncEtcdBase {
    public:
        PutRequest(const ::etcdserverpb::PutRequest& request,
                   Future fut,
                   ::etcdserverpb::KV::Stub *stub,
                   ::grpc::CompletionQueue *cq);

        void proceed() override;

    private:
        Future fut_;
        ::etcdserverpb::PutResponse response_;
        ::grpc::ClientContext context_;
        ::grpc::Status status_;
    };
};

}
