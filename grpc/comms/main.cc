#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <memory>
#include <unistd.h>
#include <sstream>
#include <thread>
#include <random>
#include <climits>
#include <algorithm>

extern "C" {
#include "comms.h"
}

#define COMMS_PACKET_CAPACITY (8192)
#define COMMS_PAYLOAD_SIZE (96)

static int total_packets = 0;

int create_packet(comms_packet_t **packet, uint32_t capacity) {
    packet[0] = (comms_packet_t*)calloc(1, sizeof(comms_packet_t));
    if (packet[0] == NULL) {
        return 1;
    }

    packet[0]->capacity = capacity;
    packet[0]->payload = new uint8_t[capacity];
    total_packets++;

    return 0;
}

void destroy_packet(comms_packet_t *packet) {
    // Clear and free payload.
    memset(packet->payload, 0, packet->capacity);
    if (packet->payload) {
        delete packet->payload;
    }
    // Clear and free packet.
    memset(packet, 0, sizeof(comms_packet_t));
    free(packet);
}

void submit_packets(comms_t *C, uint32_t receive_thread_count) {
    char *error = NULL;
    size_t packet_count = 4096;
    comms_packet_t **packets = (comms_packet_t**)calloc(sizeof(comms_packet_t*), packet_count);

    std::vector<unsigned char> data(COMMS_PAYLOAD_SIZE);

    using random_bytes_engine = std::independent_bits_engine<
        std::default_random_engine, CHAR_BIT, unsigned char>;
    random_bytes_engine rbe;

    int total_packets_reaped = 0;
    int total_packets_sent = 0;
    auto start = std::chrono::system_clock::now();
    while (true) {
        int reaped_packet_count = comms_reap(C, packets, packet_count, &error);
        total_packets_reaped += reaped_packet_count;

        // Calculate reap rate.
        auto now = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed = now-start;
        printf("packets: %7d, reap: %.4f, submit: %.4f\n", total_packets, (total_packets_reaped / elapsed.count()), (total_packets_sent / (elapsed.count())));

        // Create new packets to ensure we hit the desired packet count.
        for (size_t index=reaped_packet_count; index<packet_count; index++) {
            create_packet(&packets[index], COMMS_PACKET_CAPACITY);
        }

        // Generate random payloads.
        for (size_t index=0; index<packet_count; index++) {
            packets[index]->dst = index % receive_thread_count;

            std::generate(begin(data), end(data), std::ref(rbe));
            char *payload = (char*)data.data();
            memcpy(packets[index]->payload, payload, COMMS_PAYLOAD_SIZE);
            packets[index]->size = COMMS_PAYLOAD_SIZE;
        }

        // Submit packets.
        comms_submit(C, packets, packet_count, &error);
        total_packets_sent += packet_count;

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main(int argc, char **argv) {
    comms_t *C = NULL;
    char *error = NULL;

    // Configuration data.
    const int receive_thread_count = 1;
    const int send_thread_count = 1;
    const int base_port = 12345;
    const int lane_count = 1;
    const int send_batch_dequeue = 2048;
    const int local_index = argc > 1 ? atoi(argv[1]) : 0;

    // Create end points.
    comms_end_point_t end_point_list[] = {
        {
            .name = (char*)"me[0]",
            .address = (char*)"127.0.0.1:12345"
        },
        /*{
            .name = (char*)"me[1]",
            .address = (char*)"127.0.0.1:12346"
        },
        {
            .name = (char*)"me[1]",
            .address = (char*)"127.0.0.1:12347"
        },
        {
            .name = (char*)"me[1]",
            .address = (char*)"127.0.0.1:12348"
        },
        {
            .name = (char*)"me[1]",
            .address = (char*)"127.0.0.1:12349"
        },
        {
            .name = (char*)"me[1]",
            .address = (char*)"127.0.0.1:12350"
        },*/
    };
    const size_t end_point_count = sizeof(end_point_list) / sizeof(comms_end_point_t);

    // Create comms.
    comms_create(&C, end_point_list, end_point_count, local_index, 1, &error);

    // Configure comms.
    comms_configure(C, "process-name", "driver", &error);
    comms_configure(C, "base-port", std::to_string(base_port).c_str(), &error);
    comms_configure(C, "receive-thread-count", std::to_string(receive_thread_count).c_str(), &error);
    comms_configure(C, "send-thread-count", std::to_string(send_thread_count).c_str(), &error);
    comms_configure(C, "send-batch-dequeue", std::to_string(send_batch_dequeue).c_str(), &error);

    // Start communication.
    comms_start(C, &error);

    // Launch thread to submimt packets.
    std::thread my_thread(submit_packets, C, receive_thread_count);

    // Wait until all packets are processed.
    if (my_thread.joinable()) {
        my_thread.join();
    }

    // Do not join main thread.
    sleep(1000000);

    return EXIT_SUCCESS;
}
