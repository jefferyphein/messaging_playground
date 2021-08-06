#include <iostream>
#include <cstring>
#include <sstream>
#include <memory>
#include <google/protobuf/message_lite.h>

extern "C" {
#include "comms.h"
}

#include "EndPoint.h"
#include "SafeQueue.h"

#define COMMS_SHORT_CIRCUIT (0)

static void comms_set_error(char **error, const char *str) {
    int len = strlen(str);
    error[0] = (char*)calloc(len+1, sizeof(char));
    strncpy(error[0], str, len+1);
}

typedef struct config_t {
    char *process_name;
    size_t accessor_buffer_size;

    config_t()
        : process_name(NULL)
        , accessor_buffer_size(1024)
    {}

    void destroy() {
        if (this->process_name) {
            free(this->process_name);
            this->process_name = NULL;
        }
        memset(this, 0, sizeof(config_t));
    }
} config_t;

typedef struct comms_t {
    config_t conf_;
    int lane_count_;
    std::atomic_bool started_;
    std::atomic_bool shutting_down_;
    bool shutdown_;
    std::mutex shutdown_mtx_;
    std::condition_variable shutdown_cv_;
    std::shared_ptr<SafeQueue<comms_packet_t>> submit_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> catch_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> release_queue_;
    std::vector<EndPoint> end_points_;

    comms_t(comms_end_point_t *end_point_list,
            size_t end_point_count,
            comms_end_point_t *this_end_point,
            int lane_count)
        : conf_(config_t())
        , lane_count_(lane_count)
        , started_(false)
        , shutting_down_(false)
        , shutdown_(false)
        , shutdown_cv_()
        , submit_queue_(std::make_shared<SafeQueue<comms_packet_t>>())
        , reap_queue_(std::make_shared<SafeQueue<comms_packet_t>>())
        , catch_queue_(std::make_shared<SafeQueue<comms_packet_t>>())
        , release_queue_(std::make_shared<SafeQueue<comms_packet_t>>())
    {
        this->end_points_.reserve(end_point_count);
        for (size_t index=0; index<end_point_count; index++) {
            if (COMMS_SHORT_CIRCUIT and &end_point_list[index] == this_end_point) {
                // For the local end point, short circuit the catch/reap queues.
                this->end_points_.emplace_back(&end_point_list[index],
                                               this->catch_queue_,      // submit
                                               this->reap_queue_,       // reap
                                               this->catch_queue_,      // catch
                                               this->reap_queue_);      // release
            }
            else {
                // For remote end points, we submit/reap and catch/release
                // without a short circuit.
                this->end_points_.emplace_back(&end_point_list[index],
                                               this->submit_queue_,     // submit
                                               this->reap_queue_,       // reap
                                               this->catch_queue_,      // catch
                                               this->release_queue_);   // release
            }
        }
    }

    void wait_for_shutdown() {
        std::unique_lock<std::mutex> lck(shutdown_mtx_);
        shutdown_cv_.wait(lck);
    }

    void destroy() {
        this->conf_.destroy();
    }
} comms_t;

typedef struct comms_accessor_t {
    comms_t *C_;
    size_t end_point_count_;
    size_t buffer_size_;
    std::vector<std::vector<comms_packet_t>> packet_buffer_;

    comms_accessor_t(comms_t *C)
            : C_(C)
            , end_point_count_(C->end_points_.size())
            , buffer_size_(C->conf_.accessor_buffer_size)
            , packet_buffer_(C->end_points_.size())
    {
        for (size_t index=0; index<end_point_count_; index++) {
            packet_buffer_[index].reserve(buffer_size_);
        }
    }

    // QUESTION: Can we rearrange `packet_list`? It's not "const" and so
    // rearrangement is a potential side effect.
    int submit_n(comms_packet_t packet_list[],
                 size_t packet_count) {
        for (size_t index=0; index<packet_count; index++) {
            uint32_t dst = packet_list[index].submit.dst;
            packet_buffer_[dst].push_back(packet_list[index]);
            if (packet_buffer_[dst].size() == buffer_size_) {
                this->flush_to_end_point(packet_buffer_[dst], C_->end_points_[dst]);
            }
        }

        return 0;
    }

    int flush_to_end_point(std::vector<comms_packet_t>& buffer, EndPoint& end_point) {
        end_point.submit_n(buffer);
        buffer.clear();
        return 0;
    }
} comms_accessor_t;

int comms_create(comms_t **C,
                 comms_end_point_t *this_end_point,
                 comms_end_point_t *end_point_list,
                 size_t end_point_count,
                 int lane_count,
                 char **error) {
    try {
        C[0] = new comms_t(end_point_list, end_point_count, this_end_point, lane_count);
        return 0;
    }
    catch (std::bad_alloc& e) {
        return 1;
    }
}

int comms_configure(comms_t *C,
                    const char *key,
                    const char *value,
                    char **error) {
    int len = strlen(value);
    if (strncmp(key, "process-name", 12) == 0) {
        C->conf_.process_name = (char*)calloc(len+1, sizeof(char));
        strncpy(C->conf_.process_name, value, len+1);
    }
    else if (strncmp(key, "accessor-buffer-size", 20) == 0) {
        C->conf_.accessor_buffer_size = (size_t)atoi(value);
    }
    return 0;
}

int comms_destroy(comms_t *C,
                  char **error) {
    C->destroy();
    delete C;

    // Clean up statically allocated objects from within the protobuf library.
    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}

int comms_accessor_create(comms_accessor_t **A,
                          comms_t *C,
                          int lane,
                          char **error) {
    // Verify lane.
    if (lane < 0 or lane >= C->lane_count_) {
        std::stringstream ss;
        ss << "Invalid lane number. Valid range: [0, " << C->lane_count_
           << "); Lane provided: " << lane;
        comms_set_error(error, ss.str().c_str());
        return 1;
    }

    try {
        A[0] = new comms_accessor_t(C);
        return 0;
    }
    catch (std::bad_alloc& e) {
        std::stringstream ss;
        ss << "Unable to allocate memory for accessor.";
        comms_set_error(error, ss.str().c_str());
        return 1;
    }
}

int comms_start(comms_t *C,
                char **error) {
    C->started_ = true;
    return 0;
}

int comms_wait_for_start(comms_t *C,
                         double timeout,
                         char **error) {
    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);
    std::condition_variable cv;

    // Wait until started is set.
    if (timeout == 0.0) {
        // Wake up every 5 milliseconds to check if we've started.
        while (!cv.wait_for(lck, std::chrono::milliseconds(5), [C]{ return static_cast<bool>(C->started_); }));
        return 0;
    }

    bool started;
    if (timeout > 0.0) {
        // Convert seconds to milliseconds.
        int timeout_ms = static_cast<int>(timeout * 1000);

        started = cv.wait_for(lck, std::chrono::milliseconds(timeout_ms), [C]{ return static_cast<bool>(C->started_); });
    }
    else {
        started = C->started_;
    }

    // Set error string if comms layer hasn't started.
    if (not started) {
        std::stringstream ss;
        ss << "Comms layer not started within timeout window.";
        comms_set_error(error, ss.str().c_str());
    }

    return started ? 0 : 1;
}

int comms_submit(comms_accessor_t *A,
                 comms_packet_t packet_list[],
                 size_t packet_count,
                 char **error) {
    if (A->C_->shutting_down_) {
        std::stringstream ss;
        ss << "Cannot submit packets while comms layer is shutting down.";
        comms_set_error(error, ss.str().c_str());
        return -1;
    }

    A->submit_n(packet_list, packet_count);
    return 0;
}

int comms_wait_for_shutdown(comms_t *C,
                            double timeout,
                            char **error) {
    // If we're already shut down, nothing to do here.
    if (C->shutdown_) {
        return 0;
    }

    // Wait until comms layer has completely shut down.
    if (timeout == 0.0) {
        C->wait_for_shutdown();
        return 0;
    }

    // Wait for shutdown, or a specified timeout.
    if (timeout > 0.0) {
        // Convert seconds into milliseconds.
        int timeout_ms = static_cast<int>(timeout * 1000);

        std::unique_lock<std::mutex> lck(C->shutdown_mtx_);
        C->shutdown_cv_.wait_for(lck, std::chrono::milliseconds(timeout_ms));
    }

    // Set error string if comms layer hasn't shut down yet.
    if (not C->shutdown_) {
        std::stringstream ss;
        ss << "Comms layer has not shut down yet.";
        comms_set_error(error, ss.str().c_str());
        return 1;
    }

    return 0;
}

int comms_shutdown(comms_t *C,
                   char **error) {
    C->shutting_down_ = true;
    return 0;
}

int comms_accessor_destroy(comms_accessor_t *A,
                           char **error) {
    delete A;
    return 0;
}
