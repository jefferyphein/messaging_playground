#include <iostream>
#include <cstdlib>
#include <google/protobuf/arena.h>
#include <google/protobuf/message_lite.h>
#include "arena.pb.h"

#define PAYLOAD_SIZE (128)

// 1462 allocations (libprotobuf)

int main(int argc, char **argv) {
    ::google::protobuf::ArenaOptions arena_options;
    arena_options.start_block_size = 1<<17;
    ::google::protobuf::Arena arena(arena_options); // 1 allocation

    char *payload = (char*)malloc(PAYLOAD_SIZE); // 1 allocation

    ::arena::PacketBundle *bundle = ::google::protobuf::Arena::CreateMessage<::arena::PacketBundle>(&arena);
    bundle->mutable_packet()->Reserve(1024);

    // 1024 allocations total (+1 overhead allocation)
    for (size_t index=0; index<1024; index++) {
        auto *packet = bundle->add_packet();
        packet->set_src(0);
        packet->set_tag(1);
        packet->set_payload(payload, PAYLOAD_SIZE); // 1 allocation
    }

    // 1024 allocations total
    for (size_t index=0; index<1024; index++) {
#if 0
        const ::arena::Packet& packet = bundle->packet(index);
        const size_t size = packet.payload().size();
        char *leak = (char*)malloc(size); // 1 allocation (PAYLOAD_SIZE bytes for char*)
        memcpy(leak, packet.payload().c_str(), size);
        //free(leak); // leave commented, otherwise compiler will optimize this out
#else
        auto *packet = bundle->mutable_packet(index);
        std::string *leak = packet->release_payload(); // 1 allocation (32 bytes for std::string)
        delete leak; // 2 deallocations (payload and std::string*)
#endif
    }

    free(payload); // 1 deallocation

    ::google::protobuf::ShutdownProtobufLibrary(); // 1462 deallocations (libprotobuf)

    return EXIT_SUCCESS; // 1 deallocation (arena)
}
