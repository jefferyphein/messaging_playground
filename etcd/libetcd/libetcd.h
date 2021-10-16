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

class AsyncEtcdBase {
public:
    virtual void proceed(bool ok) = 0;
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
    Response(::grpc::Status& status, ::etcdserverpb::RangeResponse& response);
    Response(::grpc::Status& status, ::etcdserverpb::PutResponse& response);
    Response(::grpc::Status& status, ::etcdserverpb::DeleteRangeResponse& response);

    size_t size() const { return values_.size(); }
    const Values& values() const { return values_; }
    Values& values() { return values_; }
    const Value& value(size_t index = 0) const;
    Value& value(size_t index = 0);

    const Values& prev_values() const { return prev_values_; }
    Values& prev_values() { return prev_values_; }
    const Value& prev_value(size_t index = 0) const { return prev_values_.at(index); }
    Value& prev_value(size_t index = 0) { return prev_values_.at(index); }

    bool ok() const;
    std::string error_message() const { return error_message_; }
    int error_code() const { return error_code_; }
    bool network_unavailable() const;

private:
    Values values_;
    Values prev_values_;
    std::string error_message_;
    int error_code_;
};

class WatchStream;
using WatchReaderWriter = ::grpc::ClientAsyncReaderWriter<::etcdserverpb::WatchRequest, ::etcdserverpb::WatchResponse>;

class Watch: public AsyncEtcdBase {
friend WatchStream;
public:
    Watch() = delete;
    Watch(WatchStream *stream, std::string key, std::string range_end = "");
    ~Watch();
    void proceed(bool ok) override;
    std::future<std::unique_ptr<Watch>> get_future();
    void cancel();
    void wait_for_cancel();

private:
    WatchStream *stream_;
    ::etcdserverpb::WatchResponse response_;
    std::promise<std::unique_ptr<Watch>> prom_;
    int64_t watch_id_;

    std::atomic_bool created_;
    std::mutex created_mtx_;
    std::condition_variable created_cv_;

    std::atomic_bool canceled_;
    std::mutex canceled_mtx_;
    std::condition_variable canceled_cv_;

    enum State {
        CREATING,
        CREATED,
        CANCELING,
        CANCELED,
        INVALID,
    };
    State state_;

    void finalize(const ::etcdserverpb::WatchResponse& response);
};

class Future {
public:
    Future();
    Response get();
    void wait() const;
    void set_value(::grpc::Status& status);
    void set_value(::grpc::Status& status,
                   ::etcdserverpb::RangeResponse& response);
    void set_value(::grpc::Status& status,
                   ::etcdserverpb::PutResponse& response);
    void set_value(::grpc::Status& status,
                   ::etcdserverpb::DeleteRangeResponse& response);

private:
    std::shared_ptr<std::promise<Response>> prom_;
    std::shared_future<Response> fut_;
};

class WatchStream: public AsyncEtcdBase {
friend Watch;
public:
    WatchStream(::etcdserverpb::Watch::Stub *stub,
                ::grpc::CompletionQueue *cq);
    ~WatchStream();
    void proceed(bool ok) override;
    void wait_for_start();
    std::future<std::unique_ptr<Watch>> create_watch(std::string key,
                                                     std::string range_end);
    void cancel_watch(Watch *watch);
    void close();
    void wait_for_close();

private:
    std::unique_ptr<WatchReaderWriter> reader_writer_;
    ::grpc::CompletionQueue *cq_;
    ::grpc::ClientContext context_;
    ::etcdserverpb::WatchResponse response_;
    ::grpc::Status status_;
    enum State {
        START,
        START_DONE,
        READ_DONE,
        WRITES_DONE_DONE,
        INVALID,
    };
    State next_state_;
    Watch *last_watch_created_;
    Watch *last_watch_canceled_;

    std::atomic_bool started_;
    std::mutex started_mtx_;
    std::condition_variable started_cv_;

    std::atomic_bool writing_;
    std::mutex writing_mtx_;
    std::condition_variable writing_cv_;

    std::atomic_bool closed_;
    std::mutex closed_mtx_;
    std::condition_variable closed_cv_;

    void handle_response_(const ::etcdserverpb::WatchResponse& response);
};

class Client {
public:
    Client(std::string address);
    ~Client();

    Future get(std::string key,
               std::string range_end = "");

    Future set(std::string key,
               std::string value,
               bool prev_kv = false);

    Future del(std::string key,
               bool prev_kv = false);

    Future del_range(std::string key,
                     std::string range_end,
                     bool prev_kv = false);

    std::unique_ptr<Watch> watch(std::string key,
                                 std::string range_end = "");

private:
    std::string address_;

    std::unique_ptr<::etcdserverpb::KV::Stub> kv_stub_;
    std::unique_ptr<::etcdserverpb::Watch::Stub> watch_stub_;

    std::unique_ptr<WatchStream> watch_stream_;

    std::unique_ptr<std::thread> cq_thread_;
    ::grpc::CompletionQueue cq_;

    void run_completion_queue_();
};

template<typename Req,
         typename Res,
         typename Stub,
         std::unique_ptr<::grpc::ClientAsyncResponseReader<Res>> (Stub::*PrepareFunc)(::grpc::ClientContext*, const Req&, ::grpc::CompletionQueue*)>
class Request: public AsyncEtcdBase {
public:
    Request(const Req& request,
            Future fut,
            Stub *stub,
            ::grpc::CompletionQueue *cq)
        : fut_(fut)
    {
        auto rpc = (stub->*PrepareFunc)(&context_, request, cq);
        rpc->StartCall();
        rpc->Finish(&response_, &status_, this);
    }

    void proceed(bool ok) override {
        if (status_.ok()) {
            fut_.set_value(status_, response_);
        }
        else {
            fut_.set_value(status_);
        }
        delete this;
    }

private:
    Future fut_;
    Res response_;
    ::grpc::ClientContext context_;
    ::grpc::Status status_;
};

using PutRequest = Request<::etcdserverpb::PutRequest, ::etcdserverpb::PutResponse, ::etcdserverpb::KV::Stub, &::etcdserverpb::KV::Stub::PrepareAsyncPut>;
using GetRequest = Request<::etcdserverpb::RangeRequest, ::etcdserverpb::RangeResponse, ::etcdserverpb::KV::Stub, &::etcdserverpb::KV::Stub::PrepareAsyncRange>;
using DelRequest = Request<::etcdserverpb::DeleteRangeRequest, ::etcdserverpb::DeleteRangeResponse, ::etcdserverpb::KV::Stub, &::etcdserverpb::KV::Stub::PrepareAsyncDeleteRange>;

}
