#include <sstream>

extern "C" {
#include "comms.h"
}
#include "comms_impl.h"

comms_accessor_t::comms_accessor_t(comms_t *C)
        : C_(C)
        , end_point_count_(C->end_points_.size())
        , buffer_size_(C->conf_.accessor_buffer_size)
        , packet_buffer_(C->end_points_.size())
{
    for (size_t index=0; index<end_point_count_; index++) {
        packet_buffer_[index].reserve(buffer_size_);
    }
}

// QUESTION: Can we rearrange `packet_list`? It's not "const" and so
// rearranging it should be valid.
int comms_accessor_t::submit_n(comms_packet_t packet_list[],
                               size_t packet_count) {
    for (size_t index=0; index<packet_count; index++) {
        uint32_t dst = packet_list[index].submit.dst;
        packet_buffer_[dst].push_back(packet_list[index]);
        if (packet_buffer_[dst].size() == buffer_size_) {
            this->flush_to_end_point(packet_buffer_[dst], C_->end_points_[dst]);
        }
    }

    return 0;
}

int comms_accessor_t::release_n(comms_packet_t packet_list[],
                                size_t packet_count) {
    C_->end_points_[0].release_n(packet_list, packet_count);
    return 0;
}

int comms_accessor_t::flush_to_end_point(std::vector<comms_packet_t>& buffer, EndPoint& end_point) {
    end_point.submit_n(buffer);
    buffer.clear();
    return 0;
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
        A[0] = new comms_accessor_t(C);
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
