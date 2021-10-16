#include "libetcd.h"

namespace libetcd {

Watch::Watch(WatchStream *stream,
             std::string key,
             std::string range_end)
    : stream_(stream)
    , created_(false)
    , canceled_(false)
    , state_(CREATING)
{}

Watch::~Watch() {
    cancel();
    wait_for_cancel();
    state_ = INVALID;
}

void Watch::proceed(bool ok) {
    if (not ok) {
        std::runtime_error("Unable to write to Watch stream.");
    }

    if (state_ == CANCELING) {
        state_ = CANCELED;
        canceled_ = true;
        canceled_cv_.notify_all();
    }
}

void Watch::finalize(const ::etcdserverpb::WatchResponse& response) {
    watch_id_ = response.watch_id();
    state_ = CREATED;
    created_ = true;
    prom_.set_value(std::unique_ptr<Watch>(this));
    created_cv_.notify_all();
}

std::future<std::unique_ptr<Watch>> Watch::get_future() {
    return prom_.get_future();
}

void Watch::cancel() {
    switch (state_) {
    case CREATING:
    case CREATED:
        state_ = CANCELING;
        stream_->cancel_watch(this);
        break;
    }
}

void Watch::wait_for_cancel() {
    std::unique_lock<std::mutex> lck(canceled_mtx_);
    if (canceled_) return;
    canceled_cv_.wait(lck);
}

}
