#include <memory>

#include "libetcd.h"

namespace libetcd {

Future::Future()
    : prom_(std::make_shared<std::promise<Response>>())
{
    fut_ = prom_->get_future().share();
}

Response Future::get() {
    return fut_.get();
}

void Future::wait() const {
    fut_.wait();
}

void Future::set_value(::grpc::Status& status) {
    prom_->set_value(Response(status));
}

void Future::set_value(::grpc::Status& status, ::etcdserverpb::RangeResponse& response) {
    prom_->set_value(Response(status, response));
}

void Future::set_value(::grpc::Status& status, ::etcdserverpb::PutResponse& response) {
    prom_->set_value(Response(status, response));
}

void Future::set_value(::grpc::Status& status, ::etcdserverpb::DeleteRangeResponse& response) {
    prom_->set_value(Response(status, response));
}

}
