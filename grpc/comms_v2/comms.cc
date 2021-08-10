#include <iostream>
#include <cstring>
#include <sstream>
#include <google/protobuf/message_lite.h>

extern "C" {
#include "comms.h"
}
#include "comms_impl.h"

#define COMMS_SHORT_CIRCUIT (0)

void comms_set_error(char **error, const char *str) {
    int len = strlen(str);
    error[0] = (char*)calloc(len+1, sizeof(char));
    strncpy(error[0], str, len+1);
}

config_t::config_t()
    : process_name(NULL)
    , base_port(0)
    , accessor_buffer_size(1024)
    , writer_buffer_size(1024)
    , reader_buffer_size(1024)
    , writer_retry_count(25)
    , writer_retry_delay(100)
    , writer_thread_count(1)
    , reader_thread_count(1)
    , arena_start_block_depth(20)
{}

void config_t::destroy() {
    if (this->process_name) {
        free(this->process_name);
        this->process_name = NULL;
    }
    memset(this, 0, sizeof(config_t));
}

comms_t::comms_t(comms_end_point_t *end_point_list,
        size_t end_point_count,
        comms_end_point_t *this_end_point,
        int lane_count)
    : conf_(config_t())
    , lane_count_(lane_count)
    , started_(false)
    , shutting_down_(false)
    , shutdown_(false)
    , writers_()
    , submit_queue_(std::make_shared<moodycamel::ConcurrentQueue<comms_packet_t>>())
    , reap_queue_(std::make_shared<moodycamel::ConcurrentQueue<comms_packet_t>>())
    , catch_queue_(std::make_shared<moodycamel::ConcurrentQueue<comms_packet_t>>())
    , release_queue_(std::make_shared<moodycamel::ConcurrentQueue<comms_packet_t>>())
{
    this->end_points_.reserve(end_point_count);
    for (size_t index=0; index<end_point_count; index++) {
        if (COMMS_SHORT_CIRCUIT and &end_point_list[index] == this_end_point) {
            // For the local end point, short circuit the catch/reap queues.
            this->end_points_.emplace_back(&end_point_list[index],
                                           index,
                                           this->catch_queue_,      // submit
                                           this->reap_queue_,       // reap
                                           this->catch_queue_,      // catch
                                           this->reap_queue_);      // release
        }
        else {
            // For remote end points, we submit/reap and catch/release
            // without a short circuit.
            this->end_points_.emplace_back(&end_point_list[index],
                                           index,
                                           this->submit_queue_,     // submit
                                           this->reap_queue_,       // reap
                                           this->catch_queue_,      // catch
                                           this->release_queue_);   // release
        }
    }
}

void comms_t::start() {
    // Set arena starting block size for all end points.
    for (auto& end_point : end_points_) {
        end_point.set_arena_start_block_size(1<<conf_.arena_start_block_depth);
    }

    // First, start all readers.
    for (uint32_t index=0; index<conf_.reader_thread_count; index++) {
        auto reader = std::make_shared<comms_reader_t>(this);
        readers_.push_back(reader);
        reader->start();
    }

    // Next, start the receiver.
    std::stringstream addr;
    addr << "[::]:" << conf_.base_port;
    receiver_ = std::make_shared<comms_receiver_t>(readers_);
    receiver_->start(addr.str());

    // Lastly, start the writers.
    for (uint32_t index=0; index<conf_.writer_thread_count; index++) {
        auto writer = std::make_shared<comms_writer_t>(this);
        writers_.push_back(writer);
        writer->start(receiver_);
    }

    std::unique_lock<std::mutex> lck(started_mtx_);
    started_ = true;
    started_cv_.notify_all();
}

void comms_t::shutdown() {
    // Indicate intention to shut down.
    shutting_down_ = true;

    // First, shut down all writers.
    for (auto writer : writers_) {
        writer->shutdown();
        writer->wait_for_shutdown();
    }

    // Next, shut down the receiver.
    receiver_->shutdown();
    receiver_->wait_for_shutdown();

    // Lastly, shut down all readers.
    for (auto reader : readers_) {
        reader->shutdown();
        reader->wait_for_shutdown();
    }

    // Acquire shutdown mutex and notify shutdown.
    std::unique_lock<std::mutex> lck(shutdown_mtx_);
    shutdown_ = true;
    shutdown_cv_.notify_all();
}

bool comms_t::wait_for_start(double timeout) {
    // Wait for all writers to start.
    for (auto& writer : writers_) {
        std::unique_lock<std::mutex> lck(writer->started_mtx_);

        if (not writer->started_) {
            writer->started_cv_.wait(lck);
        }
    }

    std::unique_lock<std::mutex> lck(started_mtx_);

    if (started_) return true;

    if (timeout == 0.0) {
        started_cv_.wait(lck);
        return started_;
    }

    if (timeout > 0.0) {
        // Convert seconds to milliseconds.
        int timeout_ms = static_cast<int>(timeout * 1000);
        started_cv_.wait_for(lck, std::chrono::milliseconds(timeout_ms), [this]{ return this->started_; });
        return started_;
    }

    return started_;
}

bool comms_t::wait_for_shutdown(double timeout) {
    std::unique_lock<std::mutex> lck(shutdown_mtx_);

    if (shutdown_) return true;

    if (timeout == 0.0) {
        shutdown_cv_.wait(lck);
        return shutdown_;
    }

    if (timeout > 0.0) {
        // Convert seconds to milliseconds.
        int timeout_ms = static_cast<int>(timeout * 1000);
        shutdown_cv_.wait_for(lck, std::chrono::milliseconds(timeout_ms), [this]{ return this->shutdown_; });
        return shutdown_;
    }

    return shutdown_;
}

void comms_t::destroy() {
    this->conf_.destroy();
}

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
        std::stringstream ss;
        ss << "Unable to allocate comms.";
        comms_set_error(error, ss.str().c_str());
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
    else if (strncmp(key, "base-port", 9) == 0) {
        C->conf_.base_port = (uint16_t)atoi(value);
    }
    else if (strncmp(key, "accessor-buffer-size", 20) == 0) {
        C->conf_.accessor_buffer_size = (size_t)atoi(value);
    }
    else if (strncmp(key, "writer-buffer-size", 18) == 0) {
        C->conf_.writer_buffer_size = (size_t)atoi(value);
    }
    else if (strncmp(key, "reader-buffer-size", 18) == 0) {
        C->conf_.reader_buffer_size = (size_t)atoi(value);
    }
    else if (strncmp(key, "writer-retry-count", 18) == 0) {
        C->conf_.writer_retry_count = (size_t)atoi(value);
    }
    else if (strncmp(key, "writer-retry-delay", 18) == 0) {
        C->conf_.writer_retry_delay = (size_t)atoi(value);
    }
    else if (strncmp(key, "writer-thread-count", 19) == 0) {
        C->conf_.writer_thread_count = (uint32_t)atoi(value);
    }
    else if (strncmp(key, "reader-thread-count", 19) == 0) {
        C->conf_.reader_thread_count = (uint32_t)atoi(value);
    }
    else if (strncmp(key, "arena-start-block-depth", 19) == 0) {
        C->conf_.arena_start_block_depth = (uint32_t)atoi(value);
    }
    return 0;
}

int comms_destroy(comms_t *C,
                  char **error) {
    C->destroy();
    delete C;

    // Clean up objects statically allocated by the protobuf library.
    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}

int comms_start(comms_t *C,
                char **error) {
    C->start();
    return 0;
}

int comms_wait_for_start(comms_t *C,
                         double timeout,
                         char **error) {
    bool started = C->wait_for_start(timeout);

    // Set error string if comms layer hasn't started.
    if (not started) {
        std::stringstream ss;
        ss << "Comms layer not started within timeout window.";
        comms_set_error(error, ss.str().c_str());
    }

    return started ? 0 : 1;
}

int comms_wait_for_shutdown(comms_t *C,
                            double timeout,
                            char **error) {
    bool shutdown = C->wait_for_shutdown(timeout);

    // Set error string if comms layer hasn't shut down yet.
    if (not shutdown) {
        std::stringstream ss;
        ss << "Comms layer was not shut down.";
        comms_set_error(error, ss.str().c_str());
    }

    return shutdown ? 0 : 1;
}

int comms_submit(comms_accessor_t *A,
                 comms_packet_t packet_list[],
                 size_t packet_count,
                 char **error) {
    if (A->C_ == NULL) {
        std::stringstream ss;
        ss << "Cannot submit packets, accessor is not bound to a comms object.";
        comms_set_error(error, ss.str().c_str());
        return -1;
    }

    if (A->C_->shutting_down_) {
        std::stringstream ss;
        ss << "Cannot submit packets while comms layer is shutting down.";
        comms_set_error(error, ss.str().c_str());
        return -1;
    }

    A->submit_n(packet_list, packet_count);
    return packet_count;
}

int comms_reap(comms_accessor_t *A,
               comms_packet_t packet_list[],
               size_t packet_count,
               char **error) {
    if (A->C_ == NULL) {
        std::stringstream ss;
        ss << "Cannot reap packets, accessor not bound to a comms object.";
        comms_set_error(error, ss.str().c_str());
        return -1;
    }

    return A->C_->reap_queue_->try_dequeue_bulk(packet_list, packet_count);
}

int comms_catch(comms_accessor_t *A,
                comms_packet_t packet_list[],
                size_t packet_count,
                char **error) {
    if (A->C_ == NULL) {
        std::stringstream ss;
        ss << "Cannot catch packets, accessor not bound to a comms object.";
        comms_set_error(error, ss.str().c_str());
        return -1;
    }

    if (A->C_->shutdown_) {
        std::stringstream ss;
        ss << "Cannot catch packets, comms layer is shut down.";
        comms_set_error(error, ss.str().c_str());
        return -1;
    }
    return A->C_->catch_queue_->try_dequeue_bulk(packet_list, packet_count);
}

int comms_release(comms_accessor_t *A,
                  comms_packet_t packet_list[],
                  size_t packet_count,
                  char **error) {
    A->release_n(packet_list, packet_count);
    return 0;
}

int comms_shutdown(comms_t *C,
                   char **error) {
    C->shutdown();
    return 0;
}
