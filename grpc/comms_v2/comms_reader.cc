#include <sstream>

extern "C" {
#include "comms.h"
}
#include "comms_impl.h"

comms_reader_t::comms_reader_t(comms_t *C)
        : C_(C)
        , started_(false)
        , shutting_down_(false)
        , shutdown_(false)
        , thread_(nullptr)
{}

void comms_reader_t::start() {
    thread_ = std::make_shared<std::thread>(&comms_reader_t::run, this);
}

void comms_reader_t::run() {
    {
        std::unique_lock<std::mutex> lck(started_mtx_);
        started_ = true;
        started_cv_.notify_all();
    }

    while (true) {
        if (shutting_down_) break;
        // TODO: Do something.
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Acquire shutdown mutex and notify shutdown.
    std::unique_lock<std::mutex> lck(shutdown_mtx_);
    shutdown_ = true;
    shutdown_cv_.notify_all();
}

void comms_reader_t::wait_for_start() {
    std::unique_lock<std::mutex> lck(started_mtx_);
    if (started_) return;
    started_cv_.wait(lck);
}

void comms_reader_t::shutdown() {
    shutting_down_ = true;
}

void comms_reader_t::wait_for_shutdown() {
    std::unique_lock<std::mutex> lck(shutdown_mtx_);
    shutdown_cv_.wait(lck);
    thread_->join();
}
