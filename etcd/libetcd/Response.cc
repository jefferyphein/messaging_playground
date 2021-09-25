#include "libetcd.h"

namespace libetcd {

Response::Response(::grpc::Status& status)
    : error_message_(status.error_message())
    , error_code_(status.error_code())
    , is_range_(false)
{}

Response::Response(::grpc::Status& status,
                   ::etcdserverpb::RangeResponse& response,
                   bool is_range)
    : error_message_(status.error_message())
    , error_code_(status.error_code())
    , is_range_(is_range)
{
    values_.reserve(response.kvs().size());
    for (const auto& kv : response.kvs()) {
        values_.push_back(kv);
    }

    // If no results, push an invalid Value into the vector to guarantee
    // that a call to value() remains valid.
    if (values_.size() == 0) {
        values_.emplace_back(Value());
    }
}

Response::Response(::grpc::Status& status,
                   ::etcdserverpb::PutResponse& response)
    : error_message_(status.error_message())
    , error_code_(status.error_code())
{
    prev_value_ = response.prev_kv();
}

const Value& Response::value(size_t index) const {
    return values_.at(index);
}

Value& Response::value(size_t index) {
    return values_.at(index);
}

bool Response::ok() const {
    return error_code_ == ::grpc::StatusCode::OK;
}

bool Response::network_unavailable() const {
    return error_code_ == ::grpc::StatusCode::UNAVAILABLE;
}

}
