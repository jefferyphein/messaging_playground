#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <thread>
#include <vector>
#include <random>
#include <climits>
#include <algorithm>
#include <cstring>

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

#define COMMS_PAYLOAD_SIZE (96)
#define COMMS_CHECKPOINT_DELTA (0.25)

//void reader_thread(comms_t *C, const char *address) {
//    comms_reader_t *R = NULL;
//    char *error = NULL;
//    int rc;
//
//    rc = comms_wait_for_start(C, 0.0, &error);
//    COMMS_HANDLE_ERROR(rc, error);
//
//    rc = comms_reader_create(&R, C, address, &error);
//    COMMS_HANDLE_ERROR(rc, error);
//
//    rc = comms_reader_start(R, &error);
//    COMMS_HANDLE_ERROR(rc, error);
//
//    rc = comms_reader_destroy(R, &error);
//    COMMS_HANDLE_ERROR(rc, error);
//}
//
//void writer_thread(comms_t *C) {
//    comms_writer_t *W = NULL;
//    char *error = NULL;
//    int rc;
//
//    rc = comms_wait_for_start(C, 0.0, &error);
//    COMMS_HANDLE_ERROR(rc, error);
//
//    rc = comms_writer_create(&W, C, &error);
//    COMMS_HANDLE_ERROR(rc, error);
//
//    rc = comms_writer_start(W, &error);
//    COMMS_HANDLE_ERROR(rc, error);
//
//    rc = comms_writer_destroy(W, &error);
//    COMMS_HANDLE_ERROR(rc, error);
//}

int catch_and_release_thread(comms_accessor_t *A, comms_t *C) {
    char *error = NULL;
    int rc;
    const size_t packet_count = 1024;
    comms_packet_t packet_list[packet_count];

    rc = comms_wait_for_start(C, 0.0, &error);
    COMMS_HANDLE_ERROR(rc, error);

    while (true) {
        int num_caught = comms_catch(A, packet_list, packet_count, &error);
        if (num_caught == 0) continue;

        comms_release(A, packet_list, num_caught, &error);
    }
}

int main(int argc, char **argv) {
    comms_t *C = NULL;
    char *error = NULL;
    int rc = 0;
    comms_accessor_t *A = NULL;

    // Define end points.
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
    rc = comms_configure(C, "base-port", "50000", &error);
    COMMS_HANDLE_ERROR(rc, error);
    rc = comms_configure(C, "accessor-buffer-size", "1024", &error);
    COMMS_HANDLE_ERROR(rc, error);
    rc = comms_configure(C, "writer-buffer-size", "1024", &error);
    COMMS_HANDLE_ERROR(rc, error);
    rc = comms_configure(C, "writer-retry-count", "25", &error);
    COMMS_HANDLE_ERROR(rc, error);
    rc = comms_configure(C, "writer-retry-delay", "100", &error);
    COMMS_HANDLE_ERROR(rc, error);
    rc = comms_configure(C, "writer-thread-count", "1", &error);
    COMMS_HANDLE_ERROR(rc, error);
    rc = comms_configure(C, "reader-thread-count", "1", &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Create accessor.
    rc = comms_accessor_create(&A, C, 0, &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Launch reader/writer threads.
    //std::thread reader(reader_thread, C, "[::]:50000");
    //std::thread writer(writer_thread, C);
    std::thread catch_and_release(catch_and_release_thread, A, C);

    // Start the comms layer.
    rc = comms_start(C, &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Wait until comms layer has started.
    rc = comms_wait_for_start(C, 0.0, &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Generate a random payload.
    using random_bytes_engine = std::independent_bits_engine<std::default_random_engine, CHAR_BIT, unsigned char>;
    random_bytes_engine rbe;
    std::vector<unsigned char> data(COMMS_PAYLOAD_SIZE);
    std::generate(begin(data), end(data), std::ref(rbe));

    // Start timer.
    auto start = std::chrono::system_clock::now();
    double checkpoint = COMMS_CHECKPOINT_DELTA;

    // Stack-allocate some packets and submit them.
    const size_t packet_count = 1<<10;
    long total_submitted = 0;
    long total_reaped = 0;
    for (size_t n=0; n<24; n++) {
        comms_packet_t packet_list[packet_count];
        for (size_t index=0; index<packet_count; index++) {
            packet_list[index].submit.size = COMMS_PAYLOAD_SIZE;
            packet_list[index].submit.dst = 0;
            packet_list[index].submit.tag = 0;

            uint8_t *payload = (unsigned char*)calloc(COMMS_PAYLOAD_SIZE, sizeof(unsigned char));
            memcpy(payload, data.data(), sizeof(uint8_t));
            packet_list[index].payload = payload;
        }

        int packets_submitted = comms_submit(A, packet_list, packet_count, &error);
        if (packets_submitted < 0) {
            COMMS_HANDLE_ERROR(packets_submitted, error);
        }
        total_submitted += packets_submitted;
    }

    // Only use existing packets from this point forward.
    while (true) {
        comms_packet_t packet_list[packet_count];
        size_t num_reaped = comms_reap(A, packet_list, packet_count, &error);
        if (num_reaped == 0) continue;
        total_reaped += num_reaped;

        for (size_t index=0; index<num_reaped; index++) {
            packet_list[index].submit.size = COMMS_PAYLOAD_SIZE;
            packet_list[index].submit.dst = 0;
            packet_list[index].submit.tag = 0;
        }

        int packets_submitted = comms_submit(A, packet_list, packet_count, &error);
        if (packets_submitted < 0) {
            COMMS_HANDLE_ERROR(packets_submitted, error);
        }
        total_submitted += packets_submitted;

        auto now = std::chrono::system_clock::now();
        std::chrono::duration<double> diff = now-start;
        double elapsed = diff.count();
        if (elapsed >= checkpoint) {
            printf("packets reaped: %8ld, rate: %.4f\n", total_reaped, (total_reaped / elapsed));
            checkpoint = elapsed+COMMS_CHECKPOINT_DELTA;
        }
    }

    // Reap all packets and free them.
    int packets_freed = 0;
    while (total_reaped < total_submitted) {
        comms_packet_t packet_list[packet_count];
        size_t num_reaped = comms_reap(A, packet_list, packet_count, &error);
        if (num_reaped == 0) continue;
        total_reaped += num_reaped;

        for (size_t index=0; index<num_reaped; index++) {
            free(packet_list[index].payload);
            packets_freed++;
        }
    }

    comms_shutdown(C, &error);

    // Wait until shutdown command is issued.
    rc = comms_wait_for_shutdown(C, 0.0, &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Destroy accessor.
    rc = comms_accessor_destroy(A, &error);
    COMMS_HANDLE_ERROR(rc, error);

    // Destroy the comms object.
    rc = comms_destroy(C, &error);
    COMMS_HANDLE_ERROR(rc, error);

    return EXIT_SUCCESS;
}
