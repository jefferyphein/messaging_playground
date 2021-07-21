#include <iostream>
#include "Sender.h"

Sender::Sender(std::shared_ptr<SafeQueue<comms_packet_t>> send_queue,
               std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue,
               const std::vector<EndPoint>& end_points) :
        thread_(&Sender::run_sender, this, send_queue, reap_queue, std::cref(end_points)) {
}

void Sender::run_sender(std::shared_ptr<SafeQueue<comms_packet_t>> send_queue,
                        std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue,
                        const std::vector<EndPoint>& end_points) {
    while (true) {
        comms_packet_t *packet = send_queue->dequeue(1000);
        if (packet) {
            ++this->send_count_;
            //end_points[packet->dst].send(packet, reap_queue);
        }
    }
}
