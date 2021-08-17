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
        , buffer_size_(C->conf_.accessor_buffer_size)
        , submit_bundles_()
        , reap_queue_(std::make_shared<moodycamel::ConcurrentQueue<comms_packet_t,CommsPacketTraits>>(1<<21))
{
    for (auto& end_point : C_->end_points_) {
        submit_bundles_.emplace_back(end_point.is_local() ? nullptr : reap_queue_);
    }
}

void comms_accessor_t::submit_n(comms_packet_t packet_list[],
                                size_t packet_count) {
    for (size_t index=0; index<packet_count; index++) {
        uint32_t dst = packet_list[index].submit.dst;
        comms_bundle_t& bundle = submit_bundles_[dst];
        bundle.add(packet_list[index]);

        if (bundle.size() == buffer_size_) {
            // Buffer is full, submit the packets and set the return code based
            // on whether the deposit succeeded or failed.
            bool ok = C_->end_points_[dst].deposit_n(bundle);
            bundle.set_reap_rc(ok ? 0 : 1);

            // Place the packets into the reap queue, only if this is the local end point.
            if (bundle.return_queue() == nullptr) {
                // TODO: Come up with a better way to handle failure in this case.
                while (not reap_queue_->try_enqueue_bulk(bundle.packet_list(), bundle.size())) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }

            // Once we're done, clear the bundle.
            bundle.clear();
        }
    }
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
        for (size_t index=0; index<count; index++) {
            packet_list[num_caught+index] = packets[index];
        }
        num_caught += count;

        // Extra packets leftover in the bundle.
        if (count < bundle.size()) {
            bool ok = catch_queue_.try_enqueue_bulk(packet_list+count, bundle.size()-count);
            if (not ok) { std::cout << "catch: not ok" << std::endl; }
        }
    }
    return num_caught;
}

void comms_accessor_t::release_n(comms_packet_t packet_list[],
                                size_t packet_count) {
    //for (size_t index=0; index<packet_count; index++) {
    //    uint32_t src = packet_list[index].caught.src;
    //    comms_bundle_t& bundle = release_bundles_[src];
    //    bundle.add(packet_list[index]);
    //    if (bundle.size() == buffer_size_) {
    //        C_->end_points_[src].release_n(bundle);
    //        bundle.clear();
    //    }
    //}
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
