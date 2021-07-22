#pragma once

#include <thread>
#include <memory>
#include <unistd.h>

#include "EndPoint.h"
#include "SafeQueue.h"

extern "C" {
#include "comms.h"
}

class Sender {
public:
    Sender() = delete;
    Sender(std::shared_ptr<SafeQueue<comms_packet_t>> send_queue,
           std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue,
           const std::vector<EndPoint>& end_points,
           size_t packet_count);
    ~Sender();

    int count() const { return send_count_; }

    void shutdown();

private:
    void run_sender(std::shared_ptr<SafeQueue<comms_packet_t>> send_queue,
                    std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue,
                    const std::vector<EndPoint>& end_points,
                    size_t packet_count);

private:
    std::thread thread_;
    int send_count_;
    std::atomic_bool shutdown_;
};
