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
        , thread_(nullptr) {
}

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

    const size_t retry_count = C_->conf_.writer_retry_count;
    const size_t retry_delay = C_->conf_.writer_retry_delay;

    while (true) {
        // Grab a bundle.
        comms_bundle_t bundle;
        bool ok = C_->submit_queue_->try_dequeue(bundle);
        if (not ok) {
            if (shutting_down_) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Transmit the packet bundle over the wire, then set the return code.
        ok = C_->end_points_[0].transmit_n(bundle, retry_count, retry_delay);

        comms_packet_t *packet_list = bundle.packet_list();
        size_t num_packets = bundle.size();
        for (size_t index=0; index<num_packets; index++) {
            packet_list[index].reap.rc = ok ? 0 : 1;
            static_cast<PacketQueue*>(packet_list[index].opaque)->try_enqueue(packet_list[index]);
        }

        //// Deposit into the return queue only if the return queue is set.
        //if (bundle.return_queue() != nullptr) {
        //    // TODO: Probably shouldn't spin here, but alas.
        //    while (not bundle.return_queue()->try_enqueue_bulk(packet_list, num_packets)) {
        //        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        //    }
        //}
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
