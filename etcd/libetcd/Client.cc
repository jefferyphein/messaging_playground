#include "libetcd.h"

namespace libetcd {

Client::Client(std::string address)
        : address_(address)
        , kv_stub_(::etcdserverpb::KV::NewStub(::grpc::CreateChannel(address, ::grpc::InsecureChannelCredentials())))
{
    cq_thread_ = std::unique_ptr<std::thread>(new std::thread(&Client::run_completion_queue_, this));
}

Client::~Client() {
    if (cq_thread_) {
        cq_.Shutdown();
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

    new Request(request, fut, kv_stub_.get(), &cq_);
    return fut;
}

void Client::run_completion_queue_() {
    void *tag;
    bool ok = false;
    while (cq_.Next(&tag, &ok)) {
        static_cast<AsyncEtcdBase*>(tag)->proceed();
    }
}

Client::Request::Request(const ::etcdserverpb::RangeRequest &request,
                         Future fut,
                         ::etcdserverpb::KV::Stub *stub,
                         ::grpc::CompletionQueue *cq)
        : fut_(fut)
{
    auto rpc = stub->PrepareAsyncRange(&context_, request, cq);
    rpc->StartCall();
    rpc->Finish(&response_, &status_, this);
}

void Client::Request::proceed() {
    if (status_.ok()) {
        fut_.set_value(status_, response_);
    }
    else {
        fut_.set_value(status_);
    }

    delete this;
}

}
