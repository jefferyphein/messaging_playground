#include <iostream>

#include <grpcpp/grpcpp.h>

#include "Sender.h"

#include "comms.grpc.pb.h"

Sender::Sender(comms_end_point_t *end_point) :
        stub_(comms::Comms::NewStub(grpc::CreateChannel(end_point->address, grpc::InsecureChannelCredentials()))),
        name_(end_point->name),
        address_(end_point->address)
{}

void Sender::send(char *process_name) {
    comms::Packet packet;
    packet.set_src(0);
    packet.set_lane(2);
    packet.set_payload("123456");

    comms::PacketResponse response;
    grpc::ClientContext context;
    grpc::Status status = this->stub_->Send(&context, packet, &response);

    if (status.ok()) {
        std::cout << "[" << process_name << "] Packet sent successfully!" << std::endl;
    }
    else {
        std::cout << "[" << process_name << "] RPC failed: error " << status.error_code() << ": " << status.error_message() << std::endl;
    }
}
