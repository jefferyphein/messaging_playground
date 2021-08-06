#include <iostream>

#include <grpcpp/grpcpp.h>
#include <google/protobuf/arena.h>

#include "EndPoint.h"

#include "comms.grpc.pb.h"

extern "C" {
#include "comms.h"
}

EndPoint::EndPoint(comms_end_point_t *end_point)
    : stub_(comms::Comms::NewStub(grpc::CreateChannel(end_point->address, grpc::InsecureChannelCredentials())))
    , cq_(new grpc::CompletionQueue())
    , name_(end_point->name)
    , address_(end_point->address)
{}

void EndPoint::send(comms_packet_t *comms_packet,
                    std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue) {
    // Build the protobuf packets list.
    comms::Packets packets;
    auto packet = packets.add_packet();
    packet->set_src(comms_packet->src);
    packet->set_lane(comms_packet->lane);
    packet->set_payload(comms_packet->payload, comms_packet->size);

    // Reap the comms packet.
    reap_queue->enqueue(comms_packet);

    // Send the packet.
    comms::PacketResponse response;
    grpc::ClientContext context;
    grpc::Status status = this->stub_->Send(&context, packets, &response);

    if (!status.ok()) {
        std::cerr << "[" << name_ << "] RPC failed: error " << status.error_code() << ": " << status.error_message() << std::endl;
    }
}

void EndPoint::send_n(comms_packet_t **packet_list,
                      size_t packet_count,
                      std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue) {
    // Nothing to do.
    if (packet_count == 0) {
        return;
    }

    // Set up the arena allocation.
    google::protobuf::ArenaOptions arena_options;
    arena_options.start_block_size = 1<<20;
    google::protobuf::Arena arena(arena_options);
    comms::Packets *packets = google::protobuf::Arena::CreateMessage<comms::Packets>(&arena);
    packets->mutable_packet()->Reserve(packet_count);

    // Build the protobuf packets list.
    for (size_t index=0; index<packet_count; index++) {
        comms_packet_t *comms_packet = packet_list[index];
        auto *packet = packets->add_packet();
        packet->set_src(comms_packet->src);
        packet->set_lane(comms_packet->lane);
        packet->set_payload(comms_packet->payload, comms_packet->size);
    }
    reap_queue->enqueue_n(packet_list, packet_count);

#if 1
    // Send the packets.
    comms::PacketResponse response;
    grpc::ClientContext context;
    grpc::Status status;

    std::unique_ptr<grpc::ClientAsyncResponseReader<comms::PacketResponse>> rpc(
        this->stub_->PrepareAsyncSend(&context, *packets, this->cq_.get()));
    rpc->StartCall();
    rpc->Finish(&response, &status, (void*)1);
    void *got_tag;
    bool ok = false;
    GPR_ASSERT(this->cq_->Next(&got_tag, &ok));
    GPR_ASSERT(got_tag == (void*)1);
    GPR_ASSERT(ok);

    if (!status.ok()) {
        std::cerr << "[" << name_ << "] RPC failed: error " << status.error_code() << ": " << status.error_message() << std::endl;
    }
#endif
}
