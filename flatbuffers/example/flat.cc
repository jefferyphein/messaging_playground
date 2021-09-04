#include <iostream>
#include "flat_generated.h"

#define PAYLOAD_SIZE (128)

int main(int argc, char **argv) {
    ::flatbuffers::FlatBufferBuilder builder(1<<18);

    uint8_t *payload = (uint8_t*)calloc(PAYLOAD_SIZE, sizeof(uint8_t)); // 1 allocation
    for (int n=0; n<PAYLOAD_SIZE; n++) payload[n] = n;

    auto packet_bundle = ::flat::CreatePacketBundle(builder); // 1 allocation
    std::vector<::flatbuffers::Offset<::flat::Packet>> packet_vector;
    packet_vector.reserve(1024); // 1 allocation

    // 1 allocation total
    for (size_t index=0; index<1024; index++) {
        // Create Packet object.
        auto packet_payload = builder.CreateVector(payload, PAYLOAD_SIZE);
        auto packet = ::flat::CreatePacket(builder, packet_payload);
        builder.Finish(packet);

        // Put Packet object into PacketBundle.
        packet_vector.push_back(packet);
    }
    builder.Finish(packet_bundle);

    // Acquire the serialized buffer and its size.
    uint8_t *buf = builder.GetBufferPointer(); // 1 allocation
    int size = builder.GetSize();

    free(payload);

    return EXIT_SUCCESS;
}
