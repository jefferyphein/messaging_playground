#include <thread>
#include <grpcpp/grpcpp.h>
#include <google/protobuf/arena.h>

#include "comms.grpc.pb.h"

#include "EndPoint.h"

EndPoint::EndPoint(comms_end_point_t *end_point,
                   size_t end_point_id,
                   std::shared_ptr<moodycamel::ConcurrentQueue<comms_packet_t>> submit_queue,
                   std::shared_ptr<moodycamel::ConcurrentQueue<comms_packet_t>> reap_queue,
                   std::shared_ptr<moodycamel::ConcurrentQueue<comms_packet_t>> catch_queue,
                   std::shared_ptr<moodycamel::ConcurrentQueue<comms_packet_t>> release_queue,
                   uint32_t arena_start_block_depth)
        : name_(end_point->name)
        , address_(end_point->address)
        , id_(end_point_id)
        , stub_(comms::Comms::NewStub(grpc::CreateChannel(end_point->address, grpc::InsecureChannelCredentials())))
        , submit_queue_(submit_queue)
        , reap_queue_(reap_queue)
        , catch_queue_(catch_queue)
        , release_queue_(release_queue)
        , arena_start_block_size_(1<<arena_start_block_depth)
{}

void EndPoint::set_arena_start_block_size(size_t block_size) {
    arena_start_block_size_ = block_size;
}

size_t EndPoint::submit_n(const std::vector<comms_packet_t>& packet_list) {
    submit_queue_->enqueue_bulk(reinterpret_cast<const comms_packet_t*>(packet_list.data()), packet_list.size());

    return 0;
}

size_t EndPoint::release_n(comms_packet_t packet_list[], size_t packet_count) {
    release_queue_->enqueue_bulk(packet_list, packet_count);
    return 0;
}

bool EndPoint::transmit_n(const comms_packet_t packet_list[],
                          size_t packet_count,
                          size_t retry_count,
                          size_t retry_delay) {
    google::protobuf::ArenaOptions arena_options;
    arena_options.start_block_size = arena_start_block_size_;
    google::protobuf::Arena arena(arena_options);
    comms::Packets *packets = google::protobuf::Arena::CreateMessage<comms::Packets>(&arena);
    packets->mutable_packet()->Reserve(packet_count);

    for (size_t index=0; index<packet_count; index++) {
        const comms_packet_t *comms_packet = &packet_list[index];
        auto *packet = packets->add_packet();
        packet->set_src(comms_packet->submit.dst);
        packet->set_payload(comms_packet->payload, comms_packet->submit.size);
    }

    comms::PacketResponse response;
    grpc::Status status;

    size_t retry = 0;
    while (retry <= retry_count) {
        status = send_packets_internal(*packets, response);
        if (status.ok()) break;
        ++retry;
        std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay));
    }

    reap_queue_->enqueue_bulk(packet_list, packet_count);

    if (!status.ok()) {
        std::cerr << "[" << name_ << "] RPC failed: error " << status.error_code()
                  << ": " << status.error_message() << std::endl;
        return false;
    }

    return true;
}

grpc::Status EndPoint::send_packets_internal(comms::Packets& packets,
                                             comms::PacketResponse& response) {
    grpc::ClientContext context;
    return stub_->Send(&context, packets, &response);
}
