#include <iostream>
#include "flat_generated.h"

#define PAYLOAD_SIZE (1024)

size_t deserialize_packet_bundle(uint8_t *buffer) {
    auto packet_bundle = ::flat::GetPacketBundle(buffer);
    auto packets = packet_bundle->packet();
    size_t count = packets->size();

    std::vector<const uint8_t*> vec;
    vec.reserve(count);
    for (size_t index=0; index<count; index++) {
        auto packet = packets->Get(index);
        const uint8_t *ptr = packet->payload()->data();
        vec.push_back(ptr);
    }
    return vec.size();
}

void run() {
    ::flatbuffers::FlatBufferBuilder builder(1<<21);

    uint8_t *payload = (uint8_t*)calloc(PAYLOAD_SIZE, sizeof(uint8_t));
    for (int n=0; n<PAYLOAD_SIZE; n++) payload[n] = n;

    std::vector<::flatbuffers::Offset<::flat::Packet>> packet_vector;
    packet_vector.reserve(1024);

    for (size_t index=0; index<1024; index++) {
        // Create Packet object.
        auto packet_payload = builder.CreateVector(payload, PAYLOAD_SIZE);
        auto packet = ::flat::CreatePacket(builder, packet_payload);
        builder.Finish(packet);

        // Put Packet object into PacketBundle.
        packet_vector.push_back(packet);
    }
    auto packets = builder.CreateVector(packet_vector);
    auto packet_bundle = ::flat::CreatePacketBundle(builder, packets);
    builder.Finish(packet_bundle);

    // Acquire the serialized buffer.
    uint8_t *buf = builder.GetBufferPointer();

    deserialize_packet_bundle(buf);

    free(payload);
}

int main(int argc, char **argv) {
    for (int n=0; n<1024*32; n++) {
        run();
    }
    return EXIT_SUCCESS;
}
