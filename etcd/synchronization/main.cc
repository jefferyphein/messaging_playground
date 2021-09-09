#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <google/protobuf/message_lite.h>

extern "C" {
#include "sync.h"
}

#define SYNC_PRINT_ERROR(error) { \
    std::cerr << "\033[1;31mSYNC_ERROR\033[0m \033[0;32m(" << __FILE__ \
              << ":" << __LINE__ << ")\033[0m: " << (error) << std::endl; \
}

#define SYNC_HANDLE_ERROR(rc, error) { \
    if ((rc)) { \
        SYNC_PRINT_ERROR(error); \
        exit(1); \
    } \
}

int main(int argc, char **argv) {
    sync_t *S = NULL;
    char *error = NULL;
    int rc = 0;

    // Create synchronization object.
    rc = sync_create(&S, 0, 1, &error);
    SYNC_HANDLE_ERROR(rc, error);

    // Set configuration.
    rc = sync_configure(S, "remote-host", "127.0.0.1:2379", &error);
    SYNC_HANDLE_ERROR(rc, error);
    rc = sync_configure(S, "lease-ttl", "15", &error);
    SYNC_HANDLE_ERROR(rc, error);
    rc = sync_configure(S, "lease-heartbeat", "2", &error);
    SYNC_HANDLE_ERROR(rc, error);
    rc = sync_configure(S, "key-prefix", "/driver/state/", &error);
    SYNC_HANDLE_ERROR(rc, error);

    // Initialize synchronization.
    rc = sync_initialize(S, &error);
    SYNC_HANDLE_ERROR(rc, error);

    // Set state.
    std::this_thread::sleep_for(std::chrono::seconds(2));
    rc = sync_set_state(S, 1, &error);
    SYNC_HANDLE_ERROR(rc, error);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    rc = sync_set_state(S, 2, &error);
    SYNC_HANDLE_ERROR(rc, error);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Destroy synchronization object.
    rc = sync_destroy(S, &error);
    SYNC_HANDLE_ERROR(rc, error);

    ::google::protobuf::ShutdownProtobufLibrary();

    return EXIT_SUCCESS;
}
