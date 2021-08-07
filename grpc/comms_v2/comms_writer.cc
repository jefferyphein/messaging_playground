#include <vector>
#include <sstream>
#include <thread>

extern "C" {
#include "comms.h"
}
#include "comms_impl.h"

comms_writer_t::comms_writer_t(comms_t *C)
        : C_(C)
{}

int comms_writer_create(comms_writer_t **W, comms_t *C, char **error) {
    try {
        W[0] = new comms_writer_t(C);
        return 0;
    }
    catch (std::bad_alloc& e) {
        std::stringstream ss;
        ss << "Unable to allocate comms writer";
        comms_set_error(error, ss.str().c_str());
        return 1;
    }
}

int comms_writer_start(comms_writer_t *W, char **error) {
    comms_t *C = W->C_;
    const size_t packet_count = C->conf_.writer_buffer_size;
    const size_t retry_count = C->conf_.writer_retry_count;
    const size_t retry_delay = C->conf_.writer_retry_delay;

    comms_packet_t packet_list[packet_count];
    while (true) {
        size_t num_packets = C->submit_queue_->dequeue_n(packet_list, packet_count, 1);
        if (num_packets == 0) {
            if (C->shutting_down_) break;
            continue;
        }

        bool ok = C->end_points_[0].transmit_n(packet_list, packet_count, retry_count, retry_delay);
        for (size_t index=0; index<num_packets; index++) {
            packet_list[index].reap.rc = ok ? 0 : 1;
        }
        C->reap_queue_->enqueue_n(packet_list, num_packets);
    }

    return 0;
}

int comms_writer_destroy(comms_writer_t *W, char **error) {
    return 0;
}
