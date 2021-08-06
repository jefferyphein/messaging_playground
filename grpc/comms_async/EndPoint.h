#pragma once

#include <grpcpp/grpcpp.h>

extern "C" {
#include "comms.h"
}

#include "SafeQueue.h"

#include "comms.grpc.pb.h"

class EndPoint {
public:
    EndPoint() = delete;
    EndPoint(comms_end_point_t *end_point);

    void send(comms_packet_t *packet,
              std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue);

    void send_n(comms_packet_t **packet_list,
                size_t packet_count,
                std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue);

private:
    std::unique_ptr<comms::Comms::Stub> stub_;
    std::unique_ptr<grpc::CompletionQueue> cq_;
    std::string name_;
    std::string address_;
};
