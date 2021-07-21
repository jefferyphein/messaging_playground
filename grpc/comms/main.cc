#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <memory>
#include <unistd.h>
#include <sstream>

extern "C" {
#include "comms.h"
}

int main(int argc, char **argv) {
    // Configuration data.
    const int receive_thread_count = 1;
    const int base_port = 12345;
    const int lane_count = 1;
    const size_t end_point_count = 1;
    const int local_index = argc > 1 ? atoi(argv[1]) : 0;

    comms_t *C = NULL;
    char *error = NULL;

    // Create end points.
    comms_end_point_t end_point_list[] = {
        {
            .name = (char*)"me[0]",
            .address = (char*)"127.0.0.1:12345"
        }
    };

    // Create comms.
    comms_create(&C, end_point_list, end_point_count, local_index, 1, &error);

    // Configure each comms_t object.
    comms_configure(C, "process-name", "driver", &error);
    comms_configure(C, "base-port", "12345", &error);

    // Start communication.
    comms_start(C, &error);

    // Send packets.
    comms_submit(C, NULL, 0, &error);

    // Stop communication.
    comms_destroy(C);

    return EXIT_SUCCESS;
}
