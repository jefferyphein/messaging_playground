#include <iostream>
#include <vector>
#include <thread>
#include <sstream>

extern "C" {
#include "comms.h"
}

#include "Sender.h"
#include "Receiver.h"

using comms::Comms;

typedef struct config_t {
    char *process_name;
    uint16_t base_port;

    config_t() : process_name(NULL)
    {}

    void destroy() {
        if (this->process_name) {
            free(this->process_name);
        }
    }
} config_t;

typedef struct comms_t {
    Sender sender_;
    std::unique_ptr<Receiver> receiver_;
    config_t conf_;
    bool is_local_;
    int lane_count_;

    comms_t(comms_end_point_t *end_point, bool is_local, int lane_count) :
            sender_(end_point),
            conf_(config_t()),
            is_local_(is_local),
            lane_count_(lane_count) {
    }

    void start_receiver() {
        std::stringstream address;
        address << "[::]:" << this->conf_.base_port;

        this->receiver_ = std::unique_ptr<Receiver>(new Receiver(address.str()));
    }

    void stop_receiver() {
        this->receiver_->stop();
    }

} comms_t;

int comms_create(comms_t **C, comms_end_point_t *end_point_list, size_t end_point_count, int local_index, int lane_count, char **error) {
    for (size_t index=0; index<end_point_count; index++) {
        C[index] = new comms_t(&end_point_list[index], index == local_index, lane_count);
    }
    return 0;
}

int comms_configure(comms_t *C, const char *key, const char *value, char **error) {
    int len = strlen(value);
    if (strncmp(key, "process-name", 12) == 0) {
        C->conf_.process_name = (char*)calloc(len+1, 1);
        strncpy(C->conf_.process_name, value, len);
    }
    else if (strncmp(key, "base-port", 9) == 0) {
        C->conf_.base_port = (uint16_t)atoi(value);
    }
    return 0;
}

int comms_start(comms_t *C, char **error) {
    if (C->is_local_) {
        C->start_receiver();
    }
    return 0;
}

int comms_submit(comms_t *C, comms_packet_t **packet_list, size_t packet_count, char **error) {
    C->sender_.send(C->conf_.process_name);
    return 0;
}

int comms_destroy(comms_t *C) {
    C->stop_receiver();
    C->conf_.destroy();
    return 0;
}
