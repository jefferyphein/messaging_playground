#include <thread>
#include <grpcpp/grpcpp.h>
#include <google/protobuf/arena.h>

extern "C" {
#include "comms.h"
}
#include "comms_impl.h"

EndPoint::EndPoint(comms_end_point_t *end_point,
                   size_t end_point_id,
                   bool is_local,
                   std::shared_ptr<moodycamel::ConcurrentQueue<comms_bundle_t,CommsBundleTraits>> deposit_queue,
                   uint32_t arena_start_block_depth)
        : name_(end_point->name)
        , address_(end_point->address)
        , id_(end_point_id)
        , is_local_(is_local)
        , stub_(::comms::Comms::NewStub(::grpc::CreateChannel(end_point->address, ::grpc::InsecureChannelCredentials())))
        , deposit_queue_(deposit_queue)
        , arena_start_block_size_(1<<arena_start_block_depth) {
}

void EndPoint::set_arena_start_block_size(size_t block_size) {
    arena_start_block_size_ = block_size;
}

bool EndPoint::is_local() const {
    return is_local_;
}

bool EndPoint::deposit_n(comms_bundle_t& bundle) {
    // TODO: What should we do here? Probably shouldn't spin-wait block.
    return deposit_queue_->try_enqueue(bundle);
}

//void EndPoint::release_n(const comms_bundle_t& bundle) {
//    // TODO: What should we do here? Probably shouldn't block.
//    while (not release_queue_->try_enqueue(bundle));
//}

bool EndPoint::transmit_n(const comms_packet_t packet_list[],
                          size_t packet_count,
                          size_t retry_count,
                          size_t retry_delay) {
    ::google::protobuf::ArenaOptions arena_options;
    arena_options.start_block_size = arena_start_block_size_;
    ::google::protobuf::Arena arena(arena_options);
    ::comms::PacketBundle *packet_bundle = ::google::protobuf::Arena::CreateMessage<::comms::PacketBundle>(&arena);
    packet_bundle->set_lane(0);
    packet_bundle->mutable_packet()->Reserve(packet_count);

    for (size_t index=0; index<packet_count; index++) {
        const comms_packet_t *comms_packet = &packet_list[index];
        auto *packet = packet_bundle->add_packet();
        packet->set_src(comms_packet->submit.dst);
        packet->set_tag(comms_packet->submit.tag);
        packet->set_payload(comms_packet->payload, comms_packet->submit.size);
    }

    ::comms::PacketResponse response;
    ::grpc::Status status;

    size_t retry = 0;
    while (retry <= retry_count) {
        status = send_packets_internal(*packet_bundle, response);
        if (status.ok()) break;
        ++retry;
        std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay));
    }

    if (!status.ok()) {
        std::cerr << "[" << name_ << "] RPC failed: error " << status.error_code()
                  << ": " << status.error_message() << std::endl;
        return false;
    }

    return true;
}

::grpc::Status EndPoint::send_packets_internal(::comms::PacketBundle& packets,
                                               ::comms::PacketResponse& response) {
    ::grpc::ClientContext context;
    return stub_->Send(&context, packets, &response);
}
