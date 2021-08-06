#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cassert>

extern "C" {
#include "comms.h"
}

#define COMMS_PRINT_ERROR(error) { \
    std::cerr << "\033[1;31mCOMMS ERROR\033[0m \033[0;32m(" << __FILE__ \
              << ":" << __LINE__ << ")\033[0m: " << (error) << std::endl; \
}

#define COMMS_HANDLE_ERROR(rc, error) { \
    if ((rc)) { \
        COMMS_PRINT_ERROR(error); \
        exit(1); \
    } \
}

int main(int argc, char **argv) {
    comms_t *C = NULL;
    char *error = NULL;
    int rc = 0;
    comms_accessor_t *A = NULL;

    comms_end_point_t end_point_list[] = {
        {
            .name = (char*)"me",
            .address = (char*)"127.0.0.1:50000"
        }
    };
    const size_t end_point_count = sizeof(end_point_list) / sizeof(comms_end_point_t);

    // Create comms.
    rc = comms_create(&C, &end_point_list[0], end_point_list, end_point_count, 1, &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Configure comms.
    rc = comms_configure(C, "process-name", "driver", &error);
    COMMS_HANDLE_ERROR(rc, error);
    rc = comms_configure(C, "accessor-buffer-size", "1024", &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Create accessor.
    rc = comms_accessor_create(&A, C, 0, &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Start the comms layer.
    rc = comms_start(C, &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Wait until comms layer has started.
    rc = comms_wait_for_start(C, 1.0, &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Create some number of packets on the stack and submit them.
    const size_t packet_count = 128;
    for (size_t n=0; n<24; n++) {
        comms_packet_t packet_list[packet_count];
        for (size_t index=0; index<packet_count; index++) {
            packet_list[index].submit.size = 96;
            packet_list[index].submit.dst = 0;
            packet_list[index].submit.tag = 0;
            packet_list[index].payload = NULL;
        }

        int packets_submitted = comms_submit(A, packet_list, packet_count, &error);
        if (packets_submitted < 0) {
            COMMS_HANDLE_ERROR(packets_submitted, error);
        }
    }

    // Wait until shutdown command is issued.
    rc = comms_wait_for_shutdown(C, 0.5, &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Destroy accessor.
    rc = comms_accessor_destroy(A, &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Destroy the comms object.
    rc = comms_destroy(C, &error);
    COMMS_HANDLE_ERROR(rc, error);

    return EXIT_SUCCESS;
}
