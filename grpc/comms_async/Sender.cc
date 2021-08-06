#include <iostream>
#include "Sender.h"

Sender::Sender(std::shared_ptr<SafeQueue<comms_packet_t>> send_queue,
               std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue,
               std::shared_ptr<SafeQueue<comms_packet_t>> catch_queue,
               std::shared_ptr<SafeQueue<comms_packet_t>> release_queue,
               std::vector<EndPoint>& end_points,
               size_t packet_count) :
        thread_(&Sender::run_sender, this, send_queue, reap_queue, catch_queue, release_queue, std::ref(end_points), packet_count) {
}

void Sender::run_sender(std::shared_ptr<SafeQueue<comms_packet_t>> send_queue,
                        std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue,
                        std::shared_ptr<SafeQueue<comms_packet_t>> catch_queue,
                        std::shared_ptr<SafeQueue<comms_packet_t>> release_queue,
                        std::vector<EndPoint>& end_points,
                        size_t packet_count) {
    comms_packet_t **packet_list = (comms_packet_t**)calloc(sizeof(comms_packet_t*), packet_count);
    comms_packet_t ***batches = (comms_packet_t***)calloc(sizeof(comms_packet_t**), end_points.size());
    for (size_t index=0; index<end_points.size(); index++) {
        batches[index] = (comms_packet_t**)calloc(sizeof(comms_packet_t*), packet_count);
    }
    size_t *batch_size = (size_t*)calloc(sizeof(size_t), end_points.size());

    this->shutdown_ = false;
    while (not this->shutdown_) {
        // Clear batch counts.
        memset(batch_size, 0, sizeof(size_t)*end_points.size());

        // Grab packets to send, wake up every 1 millisecond.
        size_t count = send_queue->dequeue_n(packet_list, packet_count, 1);
        this->send_count_ += count;

        end_points[0].send_n(packet_list, count, reap_queue);
    }

    // Clean up.
    free(batch_size);
    for (size_t index=0; index<end_points.size(); index++) {
        free(batches[index]);
    }
    free(batches);
    free(packet_list);
}

void Sender::shutdown() {
    this->shutdown_ = true;

    if (this->thread_.joinable()) {
        this->thread_.join();
    }
}

Sender::~Sender() {
    this->shutdown();
}
