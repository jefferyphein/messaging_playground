#ifndef __COMMS_IMPL_H_
#define __COMMS_IMPL_H_

#include <condition_variable>
#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <grpcpp/grpcpp.h>

#include "EndPoint.h"
#include "SafeQueue.h"

void comms_set_error(char **error, const char *str);

typedef struct config_t {
    char *process_name;
    uint16_t base_port;
    size_t accessor_buffer_size;
    size_t reader_buffer_size;
    size_t writer_buffer_size;
    size_t writer_retry_count;
    size_t writer_retry_delay;
    uint32_t writer_thread_count;
    uint32_t reader_thread_count;

    config_t();
    void destroy();
} config_t;

typedef struct comms_reader_t {
    comms_t *C_;

    bool started_;
    std::mutex started_mtx_;
    std::condition_variable started_cv_;

    bool shutting_down_;
    bool shutdown_;
    std::mutex shutdown_mtx_;
    std::condition_variable shutdown_cv_;

    std::shared_ptr<std::thread> thread_;

    comms_reader_t(comms_t *C);
    void start();
    void run();
    void wait_for_start();
    void shutdown();
    void wait_for_shutdown();
} comms_reader_t;


class CommsServiceImpl final : public comms::Comms::Service {
public:
    grpc::Status Send(grpc::ServerContext *context,
                      const comms::Packets *request,
                      comms::PacketResponse *response) override;
};

typedef struct comms_receiver_t {
    bool started_;
    std::mutex started_mtx_;
    std::condition_variable started_cv_;

    bool shutting_down_;
    bool shutdown_;
    std::mutex shutdown_mtx_;
    std::condition_variable shutdown_cv_;

    std::vector<std::shared_ptr<comms_reader_t>> readers_;

    std::shared_ptr<std::thread> thread_;
    CommsServiceImpl service_;
    std::unique_ptr<grpc::Server> server_;

    comms_receiver_t(std::vector<std::shared_ptr<comms_reader_t>>& readers);
    void start(std::string address);
    void run(std::string address);
    void wait_for_start();
    void shutdown();
    void wait_for_shutdown();
} comms_receiver_t;

typedef struct comms_writer_t {
    comms_t *C_;

    bool started_;
    std::mutex started_mtx_;
    std::condition_variable started_cv_;

    bool shutting_down_;
    bool shutdown_;
    std::mutex shutdown_mtx_;
    std::condition_variable shutdown_cv_;

    std::shared_ptr<std::thread> thread_;

    comms_writer_t(comms_t *C);
    void start(std::shared_ptr<comms_receiver_t> receiver);
    void run(std::shared_ptr<comms_receiver_t> receiver);
    void shutdown();
    void wait_for_shutdown();
} comms_writer_t;

typedef struct comms_t {
    config_t conf_;
    int lane_count_;

    bool started_;
    std::mutex started_mtx_;
    std::condition_variable started_cv_;

    bool shutting_down_;
    std::mutex shutting_down_mtx_;
    std::condition_variable shutting_down_cv_;

    bool shutdown_;
    std::mutex shutdown_mtx_;
    std::condition_variable shutdown_cv_;

    std::vector<std::shared_ptr<comms_reader_t>> readers_;
    std::vector<std::thread> reader_threads_;

    std::shared_ptr<comms_receiver_t> receiver_;

    std::vector<std::shared_ptr<comms_writer_t>> writers_;
    std::vector<std::thread> writer_threads_;

    std::shared_ptr<SafeQueue<comms_packet_t>> submit_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> reap_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> catch_queue_;
    std::shared_ptr<SafeQueue<comms_packet_t>> release_queue_;
    std::vector<EndPoint> end_points_;

    comms_t(comms_end_point_t *end_point_list,
            size_t end_point_count,
            comms_end_point_t *this_end_point,
            int lane_count);
    void start();
    bool wait_for_start(double timeout);
    bool wait_for_shutdown(double timeout);
    void shutdown();
    void destroy();
} comms_t;

typedef struct comms_accessor_t {
    comms_t *C_;
    size_t end_point_count_;
    size_t buffer_size_;
    std::vector<std::vector<comms_packet_t>> packet_buffer_;

    comms_accessor_t(comms_t *C);
    int submit_n(comms_packet_t packet_list[],
                 size_t packet_count);
    int release_n(comms_packet_t packet_list[],
                  size_t packet_count);
    int flush_to_end_point(std::vector<comms_packet_t>& buffer, EndPoint& end_point);
} comms_accessor_t;

#endif // __COMMS_IMPL_H_
