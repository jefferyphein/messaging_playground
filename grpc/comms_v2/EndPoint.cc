#include <grpcpp/grpcpp.h>

#include "EndPoint.h"

EndPoint::EndPoint(comms_end_point_t *end_point,
                   std::shared_ptr<SafeQueue<comms_packet_t>> submit_queue,
                   std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue,
                   std::shared_ptr<SafeQueue<comms_packet_t>> catch_queue,
                   std::shared_ptr<SafeQueue<comms_packet_t>> release_queue)
        : name_(end_point->name)
        , address_(end_point->address)
        , stub_(comms::Comms::NewStub(grpc::CreateChannel(end_point->address, grpc::InsecureChannelCredentials())))
        , submit_queue_(submit_queue)
        , reap_queue_(reap_queue)
        , catch_queue_(catch_queue)
        , release_queue_(release_queue)
{}

size_t EndPoint::submit_n(const std::vector<comms_packet_t>& packet_list) {
    submit_queue_->enqueue_n(packet_list);

    return 0;
}
