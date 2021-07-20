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

void comms_end_point_create(comms_end_point_t *end_point, std::string name, std::string address) {
    end_point->name = new char[name.size()+1];
    end_point->address = new char[address.size()+1];
    strncpy(end_point->name, name.c_str(), name.size()+1);
    strncpy(end_point->address, address.c_str(), address.size()+1);
}

void comms_end_point_destroy(comms_end_point_t *end_point) {
    delete[] end_point->name;
    delete[] end_point->address;
}

int main(int argc, char **argv) {
    // Configuration data.
    const int receive_thread_count = 1;
    const int base_port = 12345;
    const int lane_count = 1;
    const size_t end_point_count = 1;
    const int local_index = argc > 1 ? atoi(argv[1]) : 0;

    // Create end points.
    comms_end_point_t *end_point_list = new comms_end_point_t[end_point_count];
    comms_end_point_create(&end_point_list[0], "me[0]", "127.0.0.1:12345");

    // Create comms.
    comms_t **C = new comms_t*[end_point_count];
    comms_create(C, end_point_list, end_point_count, local_index, 1, NULL);

    // Configure each comms_t object.
    for (size_t index=0; index<end_point_count; index++) {
        std::stringstream ss;
        ss << "thread_" << index;
        comms_configure(C[index], "process-name", ss.str().c_str(), NULL);
    }
    comms_configure(C[local_index], "base-port", "12345", NULL);

    // Start communication.
    for (size_t index=0; index<end_point_count; index++) {
        comms_start(C[index], NULL);
    }

    sleep(1);

    // Send packets.
    comms_submit(C[0], NULL, 0, NULL);

    // Stop communication.
    for (size_t n=0; n<end_point_count; n++) {
        comms_end_point_destroy(&end_point_list[n]);
        comms_destroy(C[n]);
    }
    delete[] end_point_list;

    return EXIT_SUCCESS;
}
