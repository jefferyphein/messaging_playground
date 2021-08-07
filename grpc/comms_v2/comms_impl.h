#ifndef __COMMS_IMPL_H_
#define __COMMS_IMPL_H_

#include <condition_variable>
#include <atomic>
#include <memory>
#include <vector>
#include <grpcpp/grpcpp.h>

#include "EndPoint.h"
#include "SafeQueue.h"

void comms_set_error(char **error, const char *str);

typedef struct config_t {
    char *process_name;
    size_t accessor_buffer_size;
    size_t reader_buffer_size;
    size_t writer_buffer_size;
    size_t writer_retry_count;
    size_t writer_retry_delay;

    config_t();
    void destroy();
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
            int lane_count);
    void wait_for_shutdown();
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

typedef struct comms_writer_t {
    comms_t *C_;

    comms_writer_t(comms_t *C);
} comms_writer_t;

class ReaderServiceImpl final : public comms::Comms::Service {
public:
    grpc::Status Send(grpc::ServerContext *context,
                      const comms::Packets *request,
                      comms::PacketResponse *response) override;
};

typedef struct comms_reader_t {
    comms_t *C_;
    const char *address_;
    ReaderServiceImpl service_;
    std::unique_ptr<grpc::Server> server_;
    std::atomic_bool server_built_;

    comms_reader_t(comms_t *C, const char *address);
} comms_reader_t;

#endif // __COMMS_IMPL_H_
