#include <iostream>

#include <grpcpp/grpcpp.h>

#include "EndPoint.h"

#include "comms.grpc.pb.h"

extern "C" {
#include "comms.h"
}

EndPoint::EndPoint(comms_end_point_t *end_point) :
        stub_(comms::Comms::NewStub(grpc::CreateChannel(end_point->address, grpc::InsecureChannelCredentials()))),
        name_(end_point->name),
        address_(end_point->address)
{}

void EndPoint::send(comms_packet_t *comms_packet,
                    std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue) const {
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
                      std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue) const {
    // Build the protobuf packets list.
    comms::Packets packets;
    for (size_t index=0; index<packet_count; index++) {
        comms_packet_t *comms_packet = packet_list[index];
        auto packet = packets.add_packet();
        packet->set_src(comms_packet->src);
        packet->set_lane(comms_packet->lane);
        packet->set_payload(comms_packet->payload, comms_packet->size);
    }
    reap_queue->enqueue_n(packet_list, packet_count);

    // Send the packets.
    comms::PacketResponse response;
    grpc::ClientContext context;
    grpc::Status status = this->stub_->Send(&context, packets, &response);

    if (!status.ok()) {
        std::cerr << "[" << name_ << "] RPC failed: error " << status.error_code() << ": " << status.error_message() << std::endl;
    }
}
