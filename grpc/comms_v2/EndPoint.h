#pragma once

#include <string>
#include <memory>

extern "C" {
#include "comms.h"
}

#include "comms.grpc.pb.h"

#include "SafeQueue.h"

class EndPoint {
public:
    EndPoint() = delete;
    EndPoint(comms_end_point_t *end_point,
             std::shared_ptr<SafeQueue<comms_packet_t>> submit_queue,
             std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue,
             std::shared_ptr<SafeQueue<comms_packet_t>> catch_queue,
             std::shared_ptr<SafeQueue<comms_packet_t>> release_queue);

    size_t submit_n(const std::vector<comms_packet_t>& packet_list);

private:
    std::string name_;
    std::string address_;
    std::unique_ptr<comms::Comms::Stub> stub_;
    std::shared_ptr<SafeQueue<comms_packet_t>> submit_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> catch_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> release_queue_;
};
