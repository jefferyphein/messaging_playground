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
#define COMMS_PAYLOAD_SIZE (4)
#define COMMS_FLUSH_DELTA (0.25)

static int total_packets = 0;
static std::vector<comms_packet_t*> all_packets;

int create_packet(comms_packet_t **packet, uint32_t capacity) {
    packet[0] = (comms_packet_t*)calloc(1, sizeof(comms_packet_t));
    if (packet[0] == NULL) {
        return 1;
    }

    packet[0]->capacity = capacity;
    packet[0]->payload = new uint8_t[capacity];
    packet[0]->size = COMMS_PAYLOAD_SIZE;
    all_packets.push_back(packet[0]);
    total_packets++;

    return 0;
}

void destroy_packet(comms_packet_t *packet) {
    // Clear and free payload.
    memset(packet->payload, 0, packet->capacity);
    if (packet->payload) {
        delete[] packet->payload;
    }
    // Clear and free packet.
    memset(packet, 0, sizeof(comms_packet_t));
    free(packet);
}

void bounded_submit_packets(comms_t *C, uint32_t receive_thread_count) {
    char *error = NULL;
    size_t packet_count = 4096;
    comms_packet_t **packets = (comms_packet_t**)calloc(sizeof(comms_packet_t*), packet_count);

    using random_bytes_engine = std::independent_bits_engine<std::default_random_engine, CHAR_BIT, unsigned char>;
    random_bytes_engine rbe;
    std::vector<unsigned char> data(COMMS_PAYLOAD_SIZE);
    std::generate(begin(data), end(data), std::ref(rbe));
    char *payload = (char*)data.data();

    comms_packet_t *packet;
    for (size_t index=0; index<1<<17; index++) {
        create_packet(&packet, COMMS_PACKET_CAPACITY);
        memcpy(packet->payload, payload, COMMS_PAYLOAD_SIZE);
        comms_put_reap(C, packet);
    }

    int total_packets_reaped = 0;
    int total_packets_sent = 0;
    auto start = std::chrono::system_clock::now();
    double checkpoint = COMMS_FLUSH_DELTA;
    while (total_packets_sent < 25000000) {
        int reap_count = comms_reap(C, packets, packet_count, &error);
        if (reap_count == 0) {
            continue;
        }
        total_packets_reaped += reap_count;

        // Submit packets.
        comms_submit(C, packets, reap_count, &error);
        total_packets_sent += reap_count;

        // Calculate reap rate.
        auto now = std::chrono::system_clock::now();
        std::chrono::duration<double> diff = now-start;
        double elapsed = diff.count();
        if (elapsed >= checkpoint) {
            printf("packets sent: %8d, rate: %.4f\n", total_packets_sent, (total_packets_sent / elapsed));
            checkpoint = elapsed+COMMS_FLUSH_DELTA;
        }
    }

    free(packets);

    comms_shutdown(C, &error);
}

int main(int argc, char **argv) {
    comms_t *C = NULL;
    char *error = NULL;

    // Configuration data.
    const int receive_thread_count = 1;
    const int send_thread_count = 1;
    const int base_port = 50000;
    const int lane_count = 1;
    const int send_batch_dequeue = 2048;
    const int local_index = argc > 1 ? atoi(argv[1]) : 0;

    // Create end points.
    comms_end_point_t end_point_list[] = {
        {
            .name = (char*)"me[0]",
            .address = (char*)"127.0.0.1:50000"
        },
        /*{
            .name = (char*)"me[1]",
            .address = (char*)"127.0.0.1:50001"
        },
        {
            .name = (char*)"me[2]",
            .address = (char*)"127.0.0.1:50002"
        },
        {
            .name = (char*)"me[3]",
            .address = (char*)"127.0.0.1:50003"
        },
        {
            .name = (char*)"me[4]",
            .address = (char*)"127.0.0.1:50004"
        },
        {
            .name = (char*)"me[5]",
            .address = (char*)"127.0.0.1:50005"
        },*/
    };
    const size_t end_point_count = sizeof(end_point_list) / sizeof(comms_end_point_t);

    // Create comms.
    comms_create(&C, end_point_list, end_point_count, local_index, lane_count, &error);

    // Configure comms.
    comms_configure(C, "process-name", "driver", &error);
    comms_configure(C, "base-port", std::to_string(base_port).c_str(), &error);
    comms_configure(C, "receive-thread-count", std::to_string(receive_thread_count).c_str(), &error);
    comms_configure(C, "send-thread-count", std::to_string(send_thread_count).c_str(), &error);
    comms_configure(C, "send-batch-dequeue", std::to_string(send_batch_dequeue).c_str(), &error);

    // Start communication.
    comms_start(C, &error);

    // Launch thread to submimt packets.
    std::thread my_thread(bounded_submit_packets, C, receive_thread_count);

    // Do not join main thread.
    comms_wait(C, &error);

    // Wait until all packets are processed.
    if (my_thread.joinable()) {
        my_thread.join();
    }

    // Destroy the comms object.
    comms_destroy(C, &error);

    // Destroy all outstanding packets.
    for (comms_packet_t *packet : all_packets) {
        destroy_packet(packet);
    }

    return EXIT_SUCCESS;
}
