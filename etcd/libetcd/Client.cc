#include "libetcd.h"

namespace libetcd {

Client::Client(std::string address)
    : address_(address)
    , kv_stub_(::etcdserverpb::KV::NewStub(::grpc::CreateChannel(address, ::grpc::InsecureChannelCredentials())))
    , watch_stub_(::etcdserverpb::Watch::NewStub(::grpc::CreateChannel(address, ::grpc::InsecureChannelCredentials())))
{
    cq_thread_ = std::unique_ptr<std::thread>(new std::thread(&Client::run_completion_queue_, this));
    watch_stream_ = std::unique_ptr<WatchStream>(new WatchStream(watch_stub_.get(), &cq_));
}

Client::~Client() {
    if (watch_stream_) {
        // Close the watch stream.
        watch_stream_->close();
        watch_stream_->wait_for_close();
    }

    // Stop the completion queue.
    cq_.Shutdown();

    if (cq_thread_) {
        cq_thread_->join();
    }
}

Future Client::get(std::string key,
                   std::string range_end) {
    Future fut;

    ::etcdserverpb::RangeRequest request;
    request.set_key(key);
    if (not range_end.empty()) {
        request.set_range_end(range_end);
    }

    new GetRequest(request, fut, kv_stub_.get(), &cq_);
    return fut;
}

Future Client::set(std::string key,
                   std::string value,
                   bool prev_kv) {
    Future fut;

    ::etcdserverpb::PutRequest request;
    request.set_key(key);
    request.set_value(value);
    request.set_prev_kv(prev_kv);

    new PutRequest(request, fut, kv_stub_.get(), &cq_);
    return fut;
}

Future Client::del(std::string key,
                   bool prev_kv) {
    return del_range(key, "", prev_kv);
}

Future Client::del_range(std::string key,
                         std::string range_end,
                         bool prev_kv) {
    Future fut;

    ::etcdserverpb::DeleteRangeRequest request;
    request.set_key(key);
    if (not range_end.empty()) {
        request.set_range_end(range_end);
    }
    request.set_prev_kv(prev_kv);

    new DelRequest(request, fut, kv_stub_.get(), &cq_);
    return fut;
}

std::unique_ptr<Watch> Client::watch(std::string key,
                                     std::string range_end) {
    return watch_stream_->create_watch(key, range_end).get();
}

void Client::run_completion_queue_() {
    void *tag;
    bool ok = false;
    while (cq_.Next(&tag, &ok)) {
        static_cast<AsyncEtcdBase*>(tag)->proceed(ok);
    }
}

}
