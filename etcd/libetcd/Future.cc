#include <memory>

#include "libetcd.h"

namespace libetcd {

Future::Future(bool is_range)
    : prom_(std::make_shared<std::promise<Response>>())
    , is_range_(is_range)
{}

Response Future::get() {
    return prom_->get_future().get();
}

void Future::set_value(::grpc::Status& status) {
    prom_->set_value(Response(status));
}

void Future::set_value(::grpc::Status& status, ::etcdserverpb::RangeResponse& response) {
    prom_->set_value(Response(status, response, is_range_));
}

void Future::set_value(::grpc::Status& status, ::etcdserverpb::PutResponse& response) {
    prom_->set_value(Response(status, response));
}

}
