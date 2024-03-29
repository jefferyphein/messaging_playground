#include <iostream>
#include <vector>
#include <thread>
#include <sstream>
#include <unistd.h>

extern "C" {
#include "comms.h"
}

#include "SafeQueue.h"
#include "EndPoint.h"
#include "Receiver.h"
#include "Sender.h"

using comms::Comms;

typedef struct config_t {
    char *process_name;
    uint16_t base_port;
    uint32_t receive_thread_count;
    uint32_t send_thread_count;
    size_t send_batch_dequeue;

    config_t() :
        process_name(NULL),
        base_port(50000),
        receive_thread_count(1),
        send_thread_count(1),
        send_batch_dequeue(1024)
    {}

    void destroy() {
        if (this->process_name) {
            free(this->process_name);
        }
        memset(this, 0, sizeof(config_t));
    }
} config_t;

typedef struct comms_t {
    std::vector<EndPoint> end_points_;
    std::vector<std::unique_ptr<Receiver>> receivers_;
    std::vector<std::unique_ptr<Sender>> senders_;
    config_t conf_;
    int lane_count_;
    std::condition_variable shutdown_;
    std::shared_ptr<SafeQueue<comms_packet_t>> send_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue_;

    comms_t(comms_end_point_t *end_point_list, size_t end_point_count, bool is_local, int lane_count) :
            conf_(config_t()),
            lane_count_(lane_count),
            shutdown_(),
            send_queue_(std::make_shared<SafeQueue<comms_packet_t>>()),
            reap_queue_(std::make_shared<SafeQueue<comms_packet_t>>()) {
        this->end_points_.reserve(end_point_count);
        for (size_t index=0; index<end_point_count; index++) {
            this->end_points_.emplace_back(&end_point_list[index]);
        }
    }

    void start_receivers() {
        receivers_.reserve(this->conf_.receive_thread_count);
        for (uint32_t index=0; index<this->conf_.receive_thread_count; index++) {
            std::stringstream address;
            address << "[::]:" << this->conf_.base_port+index;

            this->receivers_.emplace_back(new Receiver(address.str()));
        }
    }

    void start_senders() {
        senders_.reserve(this->conf_.send_thread_count);
        for (uint32_t index=0; index<this->conf_.send_thread_count; index++) {
            this->senders_.emplace_back(new Sender(this->send_queue_, this->reap_queue_, this->end_points_, this->conf_.send_batch_dequeue));
        }
    }

    void shutdown_receivers() {
        for (auto& receiver : this->receivers_) {
            receiver->shutdown();
        }
    }

    void shutdown_senders() {
        for (auto& sender : this->senders_) {
            sender->shutdown();
        }
    }

    int wait() {
        std::mutex m;
        std::unique_lock<std::mutex> lck(m);
        this->shutdown_.wait(lck);
        return 0;
    }

    void destroy() {
        this->conf_.destroy();
    }
} comms_t;

int comms_create(comms_t **C, comms_end_point_t *end_point_list, size_t end_point_count, int local_index, int lane_count, char **error) {
    try {
        C[0] = new comms_t(end_point_list, end_point_count, local_index, lane_count);
        return 0;
    }
    catch (std::bad_alloc& e) {
        return 1;
    }
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
    else if (strncmp(key, "receive-thread-count", 20) == 0) {
        C->conf_.receive_thread_count = (int)atoi(value);
    }
    else if (strncmp(key, "send-thread-count", 17) == 0) {
        C->conf_.send_thread_count = (int)atoi(value);
    }
    else if (strncmp(key, "send-batch-dequeue", 18) == 0) {
        C->conf_.send_batch_dequeue = (int)atoi(value);
    }
    return 0;
}

int comms_start(comms_t *C, char **error) {
    C->start_receivers();
    C->start_senders();
    return 0;
}

int comms_submit(comms_t *C, comms_packet_t **packet_list, size_t packet_count, char **error) {
    C->send_queue_->enqueue_n(packet_list, packet_count);
    return 0;
}

int comms_reap(comms_t *C, comms_packet_t **packet_list, size_t packet_count, char **error) {
    return C->reap_queue_->dequeue_n(packet_list, packet_count);
}

int comms_wait(comms_t *C, char **error) {
    return C->wait();
}

int comms_shutdown(comms_t *C, char **error) {
    C->shutdown_senders();
    C->shutdown_receivers();
    C->shutdown_.notify_all();
    return 0;
}

int comms_destroy(comms_t *C, char **error) {
    C->destroy();
    delete C;
    return 0;
}
