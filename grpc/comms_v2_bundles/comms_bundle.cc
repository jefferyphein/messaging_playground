extern "C" {
#include "comms.h"
}
#include "comms_impl.h"

comms_bundle_t::comms_bundle_t()
        : return_queue_(nullptr)
        , size_(0) {
}

comms_bundle_t::comms_bundle_t(std::shared_ptr<moodycamel::ConcurrentQueue<comms_packet_t,CommsPacketTraits>> return_queue)
        : return_queue_(return_queue)
        , size_(0) {
}

void comms_bundle_t::add(const comms_packet_t& packet) {
    packet_list_[size_++] = packet;
}

size_t comms_bundle_t::size() const {
    return size_;
}

void comms_bundle_t::clear() {
    size_ = 0;
}

comms_packet_t *comms_bundle_t::packet_list() {
    return packet_list_;
}

void comms_bundle_t::set_reap_rc(int rc) {
    for (size_t index=0; index<size_; index++) {
        packet_list_[index].reap.rc = rc;
    }
}

std::shared_ptr<moodycamel::ConcurrentQueue<comms_packet_t,CommsPacketTraits>> comms_bundle_t::return_queue() const {
    return return_queue_;
}
