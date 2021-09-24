#include "libetcd.h"

#include <memory>

namespace libetcd {

Future::Future()
        : prom_(std::make_shared<std::promise<Response>>())
{}

Response Future::get() {
    return prom_->get_future().get();
}

void Future::set_value(::grpc::Status& status, ::etcdserverpb::RangeResponse& response) {
    prom_->set_value(Response(status, response));
}

void Future::set_value(::grpc::Status& status) {
    prom_->set_value(Response(status));
}

}
