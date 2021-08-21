#include <sstream>

extern "C" {
#include "comms.h"
}
#include "comms_impl.h"

#include <atomic>

comms_accessor_t::comms_accessor_t(comms_t *C, int lane)
        : C_(C)
        , lane_(lane)
        , end_point_count_(C->end_points_.size())
        , buffer_size_(COMMS_BUNDLE_SIZE)
        , submit_bundles_(C->end_points_.size())
        , reap_queue_(std::make_shared<moodycamel::ConcurrentQueue<comms_packet_t,CommsPacketTraits>>(1<<21))
{}

static void comms_accessor_submit_bundle(comms_accessor_t *A, EndPoint& end_point, comms_bundle_t& bundle) {
    // Assign the reap queue to the opaque pointer for each packet in bundle.
    comms_packet_t *packet_list = bundle.packet_list();
    size_t packet_count = bundle.size();
    for (size_t index=0; index<packet_count; index++) {
        packet_list[index].opaque = (void*)A->reap_queue_.get();
    }

    // Buffer is full, submit the packets and set the return code based
    // on whether the deposit succeeded or failed.
    bool ok = end_point.deposit_n(bundle);

    // If the deposit failed, update return code and immediately place into
    // reap queue.
    if (not ok) {
        bundle.set_reap_rc(COMMS_NOT_SCHEDULED);

        while (not A->reap_queue_->try_enqueue_bulk(packet_list, packet_count)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Once we're done, clear the bundle.
    bundle.clear();
}

void comms_accessor_t::submit_n(comms_packet_t packet_list[],
                                size_t packet_count) {
    for (size_t index=0; index<packet_count; index++) {
        uint32_t dst = packet_list[index].submit.dst;
        comms_bundle_t& bundle = submit_bundles_[dst];
        bundle.add(packet_list[index]);

        if (bundle.size() == buffer_size_) {
            comms_accessor_submit_bundle(this, C_->end_points_[dst], bundle);
        }
    }
}

size_t comms_accessor_t::submit_flush() {
    size_t num_flushed = 0;
    for (size_t index=0; index<end_point_count_; index++) {
        comms_accessor_submit_bundle(this, C_->end_points_[index], submit_bundles_[index]);
        num_flushed += submit_bundles_[index].size();
    }
    return num_flushed;
}

size_t comms_accessor_t::reap_n(comms_packet_t packet_list[],
                                size_t packet_count) {
    return reap_queue_->try_dequeue_bulk(packet_list, packet_count);
}

size_t comms_accessor_t::catch_n(comms_packet_t packet_list[],
                                 size_t packet_count) {
    size_t num_caught = catch_queue_.try_dequeue_bulk(packet_list, packet_count);
    if (num_caught == packet_count) {
        return num_caught;
    }

    comms_bundle_t bundle;
    while (num_caught < packet_count) {
        bool ok = C_->catch_queue_->try_dequeue(bundle);
        if (not ok) return num_caught;

        size_t count = std::min(packet_count-num_caught, bundle.size());
        comms_packet_t *packets = bundle.packet_list();
        memcpy(packet_list+num_caught, packets, sizeof(comms_packet_t)*count);
        num_caught += count;

        // Extra packets leftover in the bundle.
        if (count < bundle.size()) {
            bool ok = catch_queue_.try_enqueue_bulk(packets+count, bundle.size()-count);
            if (not ok) { std::cout << "catch: not ok" << std::endl; }
        }
    }
    return num_caught;
}

void comms_accessor_t::release_n(comms_packet_t packet_list[],
                                 size_t packet_count) {
    for (size_t index=0; index<packet_count; index++) {
        while (not static_cast<PacketQueue*>(packet_list[index].opaque)->try_enqueue(packet_list[index])) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

int comms_accessor_create(comms_accessor_t **A,
                          comms_t *C,
                          int lane,
                          char **error) {
    // Verify lane.
    if (lane < 0 or lane >= C->lane_count_) {
        std::stringstream ss;
        ss << "Invalid lane number. Valid range: [0, " << C->lane_count_
           << "); Lane provided: " << lane;
        comms_set_error(error, ss.str().c_str());
        return 1;
    }

    try {
        A[0] = new comms_accessor_t(C, lane);
        return 0;
    }
    catch (std::bad_alloc& e) {
        std::stringstream ss;
        ss << "Unable to allocate memory for accessor.";
        comms_set_error(error, ss.str().c_str());
        return 1;
    }
}

int comms_accessor_destroy(comms_accessor_t *A,
                           char **error) {
    A->C_ = NULL;
    delete A;
    return 0;
}
