#include <iostream>
#include <cstdlib>
#include <google/protobuf/arena.h>
#include <google/protobuf/message_lite.h>
#include "arena.pb.h"

#define PAYLOAD_SIZE (1)

void run() {
    ::google::protobuf::ArenaOptions arena_options;
    arena_options.start_block_size = 1<<20;
    ::google::protobuf::Arena arena(arena_options);

    char *payload = (char*)malloc(PAYLOAD_SIZE);

    ::arena::PacketBundle *bundle = ::google::protobuf::Arena::CreateMessage<::arena::PacketBundle>(&arena);
    bundle->mutable_packet()->Reserve(1024);

    for (size_t index=0; index<1024; index++) {
        auto *packet = bundle->add_packet();
        packet->set_src(0);
        packet->set_tag(1);
        packet->set_payload(payload, PAYLOAD_SIZE);
    }

    for (size_t index=0; index<1024; index++) {
#if 0
        const ::arena::Packet& packet = bundle->packet(index);
        const size_t size = packet.payload().size();
        char *leak = (char*)malloc(size);
        memcpy(leak, packet.payload().c_str(), size);
        //free(leak); // leave commented, otherwise compiler will optimize this out
#else
        auto *packet = bundle->mutable_packet(index);
        std::string *leak = packet->release_payload();
        delete leak;
#endif
    }

    free(payload);
}

int main(int argc, char **argv) {
    for (int n=0; n<1024*32; n++) {
        run();
    }

    ::google::protobuf::ShutdownProtobufLibrary();
    return EXIT_SUCCESS;
}
