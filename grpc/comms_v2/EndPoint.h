#pragma once

#include <string>
#include <memory>

extern "C" {
#include "comms.h"
}

#include "comms.grpc.pb.h"
#include "comms.pb.h"

#include "SafeQueue.h"

class EndPoint {
public:
    EndPoint() = delete;
    EndPoint(comms_end_point_t *end_point,
             size_t end_point_id,
             std::shared_ptr<SafeQueue<comms_packet_t>> submit_queue,
             std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue,
             std::shared_ptr<SafeQueue<comms_packet_t>> catch_queue,
             std::shared_ptr<SafeQueue<comms_packet_t>> release_queue);

    size_t submit_n(const std::vector<comms_packet_t>& packet_list);
    size_t release_n(comms_packet_t packet_list[], size_t packet_count);
    bool transmit_n(const comms_packet_t packet_list[],
                    size_t packet_count,
                    size_t retry_count,
                    size_t retry_delay);

private:
    std::string name_;
    std::string address_;
    size_t id_;
    std::unique_ptr<comms::Comms::Stub> stub_;
    std::shared_ptr<SafeQueue<comms_packet_t>> submit_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> catch_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> release_queue_;

    grpc::Status send_packets_internal(comms::Packets& packets,
                                       comms::PacketResponse& response);
};
