#include "libetcd.h"

namespace libetcd {

Response::Response(::grpc::Status& status)
        : status_(status)
{}

Response::Response(::grpc::Status& status,
                   ::etcdserverpb::RangeResponse& response)
        : status_(status)
{
    values_.reserve(response.kvs().size());
    for (const auto& kv : response.kvs()) {
        values_.push_back(kv);
    }

    if (response.kvs().size() == 1) {
        value_ = response.kvs()[0];
    }
}

bool Response::ok() const {
    return status_.ok();
}

}
