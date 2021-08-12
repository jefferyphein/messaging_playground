#include <vector>
#include <sstream>
#include <thread>

extern "C" {
#include "comms.h"
}
#include "comms_impl.h"

comms_writer_t::comms_writer_t(comms_t *C)
        : C_(C)
        , started_(false)
        , shutting_down_(false)
        , shutdown_(false)
        , thread_(nullptr)
#ifdef COMMS_USE_TOKENS
        , consumer_submit_token_(*C->submit_queue_)
        , producer_reap_token_(*C->reap_queue_)
#endif
{}

void comms_writer_t::start(std::shared_ptr<comms_receiver_t> receiver) {
    thread_ = std::make_shared<std::thread>(&comms_writer_t::run, this, receiver);
}

void comms_writer_t::run(std::shared_ptr<comms_receiver_t> receiver) {
    receiver->wait_for_start();

    {
        // Notify start.
        std::unique_lock<std::mutex> lck(started_mtx_);
        started_ = true;
        started_cv_.notify_all();
    }

    const size_t packet_count = C_->conf_.writer_buffer_size;
    const size_t retry_count = C_->conf_.writer_retry_count;
    const size_t retry_delay = C_->conf_.writer_retry_delay;

    comms_packet_t packet_list[packet_count];
    while (true) {
#ifdef COMMS_USE_TOKENS
        size_t num_packets = C_->submit_queue_->try_dequeue_bulk(consumer_submit_token_, packet_list, packet_count);
#else
        size_t num_packets = C_->submit_queue_->try_dequeue_bulk(packet_list, packet_count);
#endif
        if (num_packets == 0) {
            if (shutting_down_) {
                break;
            }
            continue;
        }

        bool ok = C_->end_points_[0].transmit_n(packet_list, packet_count, retry_count, retry_delay);
        for (size_t index=0; index<num_packets; index++) {
            packet_list[index].reap.rc = ok ? 0 : 1;
        }

        while (true) {
#ifdef COMMS_USE_TOKENS
            ok = C_->reap_queue_->try_enqueue_bulk(producer_reap_token_, packet_list, num_packets);
#else
            ok = C_->reap_queue_->try_enqueue_bulk(packet_list, num_packets);
#endif
            if (ok) break;
        }
    }

    // Acquire shutdown mutex and notify shutdown.
    std::unique_lock<std::mutex> lck(shutdown_mtx_);
    shutdown_ = true;
    shutdown_cv_.notify_all();
}

void comms_writer_t::shutdown() {
    shutting_down_ = true;
}

void comms_writer_t::wait_for_shutdown() {
    std::unique_lock<std::mutex> lck(shutdown_mtx_);
    if (not shutdown_) {
        shutdown_cv_.wait(lck);
    }
    thread_->join();
}
