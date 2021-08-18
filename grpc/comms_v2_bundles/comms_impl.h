#ifndef __COMMS_IMPL_H_
#define __COMMS_IMPL_H_

#include <condition_variable>
#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <grpcpp/grpcpp.h>

#include "comms.grpc.pb.h"
#include "comms.pb.h"

#include "concurrentqueue.h"

#define COMMS_BUNDLE_SIZE (1024)
#define COMMS_SHORT_CIRCUIT (1)
#define COMMS_USE_ASYNC_SERVICE

struct CommsPacketTraits : public moodycamel::ConcurrentQueueDefaultTraits {
    static const size_t IMPLICIT_INITIAL_INDEX_SIZE = 256;
    static const size_t BLOCK_SIZE = 1024;
};

struct CommsBundleTraits : public moodycamel::ConcurrentQueueDefaultTraits {
    static const size_t IMPLICIT_INITIAL_INDEX_SIZE = 256;
    static const size_t BLOCK_SIZE = 32;
};

void comms_set_error(char **error, const char *str);
typedef struct comms_accessor_t comms_accessor_t;

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
    uint32_t arena_start_block_depth;

    config_t();
    void destroy();
} config_t;

typedef struct comms_bundle_t {
    std::shared_ptr<moodycamel::ConcurrentQueue<comms_packet_t,CommsPacketTraits>> return_queue_;
    size_t size_;
    comms_packet_t packet_list_[COMMS_BUNDLE_SIZE];

    comms_bundle_t();
    comms_bundle_t(std::shared_ptr<moodycamel::ConcurrentQueue<comms_packet_t,CommsPacketTraits>> return_queue);
    void add(const comms_packet_t& packet);
    size_t size() const;
    void clear();
    comms_packet_t *packet_list();
    void set_reap_rc(int rc);
    std::shared_ptr<moodycamel::ConcurrentQueue<comms_packet_t,CommsPacketTraits>> return_queue() const;
} comms_bundle_t;

typedef struct comms_reader_t {
    comms_t *C_;

    std::atomic_bool started_;
    std::mutex started_mtx_;
    std::condition_variable started_cv_;

    std::atomic_bool shutting_down_;
    std::atomic_bool shutdown_;
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

class CommsSyncServiceImpl final : public ::comms::Comms::Service {
public:
    ::grpc::Status Send(::grpc::ServerContext *context,
                        const ::comms::PacketBundle *request,
                        ::comms::PacketResponse *response) override;
};

typedef struct comms_receiver_t {
    std::atomic_bool started_;
    std::mutex started_mtx_;
    std::condition_variable started_cv_;

    std::atomic_bool shutting_down_;
    std::atomic_bool shutdown_;
    std::mutex shutdown_mtx_;
    std::condition_variable shutdown_cv_;

    std::vector<std::shared_ptr<comms_reader_t>> readers_;

    std::shared_ptr<std::thread> thread_;
    std::unique_ptr<::grpc::Server> server_;

#ifdef COMMS_USE_ASYNC_SERVICE
    ::comms::Comms::AsyncService service_;
    std::unique_ptr<::grpc::ServerCompletionQueue> cq_;

    class CallData {
    public:
        CallData(::comms::Comms::AsyncService *service,
                 ::grpc::ServerCompletionQueue *cq);

        void Proceed();

    private:
        ::comms::Comms::AsyncService *service_;
        ::grpc::ServerCompletionQueue *cq_;
        ::grpc::ServerContext ctx_;
        ::comms::PacketBundle request_;
        ::comms::PacketResponse response_;
        ::grpc::ServerAsyncResponseWriter<::comms::PacketResponse> responder_;
        enum CallStatus { CREATE, PROCESS, FINISH };
        CallStatus status_;
    };
#else
    CommsSyncServiceImpl service_;
#endif

    comms_receiver_t(std::vector<std::shared_ptr<comms_reader_t>>& readers);
    void start(std::string address);
    void run(std::string address);
    void wait_for_start();
    void shutdown();
    void wait_for_shutdown();
} comms_receiver_t;

typedef struct comms_writer_t {
    comms_t *C_;

    std::atomic_bool started_;
    std::mutex started_mtx_;
    std::condition_variable started_cv_;

    std::atomic_bool shutting_down_;
    std::atomic_bool shutdown_;
    std::mutex shutdown_mtx_;
    std::condition_variable shutdown_cv_;

    std::shared_ptr<std::thread> thread_;

    comms_writer_t(comms_t *C);
    void start(std::shared_ptr<comms_receiver_t> receiver);
    void run(std::shared_ptr<comms_receiver_t> receiver);
    void shutdown();
    void wait_for_shutdown();
} comms_writer_t;

class EndPoint {
public:
    EndPoint() = delete;
    EndPoint(comms_end_point_t *end_point,
             size_t end_point_id,
             bool is_local,
             std::shared_ptr<moodycamel::ConcurrentQueue<comms_bundle_t,CommsBundleTraits>> deposit_queue,
             uint32_t arena_start_block_depth = 1<<20);

    void set_arena_start_block_size(size_t block_size);

    bool deposit_n(comms_bundle_t& bundle);
    void release_n(comms_bundle_t& bundle);
    bool transmit_n(comms_bundle_t& bundle,
                    size_t retry_count,
                    size_t retry_delay);
    bool is_local() const;

private:
    std::string name_;
    std::string address_;
    size_t id_;
    bool is_local_;
    std::unique_ptr<::comms::Comms::Stub> stub_;
    std::shared_ptr<moodycamel::ConcurrentQueue<comms_bundle_t,CommsBundleTraits>> deposit_queue_;
    size_t arena_start_block_size_;

    ::grpc::Status send_packets_internal(::comms::PacketBundle& packets,
                                         ::comms::PacketResponse& response);
};

typedef struct comms_t {
    config_t conf_;
    int lane_count_;

    std::atomic_bool started_;
    std::mutex started_mtx_;
    std::condition_variable started_cv_;

    std::atomic_bool shutting_down_;
    std::atomic_bool shutdown_;
    std::mutex shutdown_mtx_;
    std::condition_variable shutdown_cv_;

    std::vector<std::shared_ptr<comms_reader_t>> readers_;
    std::vector<std::thread> reader_threads_;

    std::shared_ptr<comms_receiver_t> receiver_;

    std::vector<std::shared_ptr<comms_writer_t>> writers_;
    std::vector<std::thread> writer_threads_;

    std::shared_ptr<moodycamel::ConcurrentQueue<comms_bundle_t,CommsBundleTraits>> submit_queue_;
    std::shared_ptr<moodycamel::ConcurrentQueue<comms_bundle_t,CommsBundleTraits>> catch_queue_;

    std::vector<EndPoint> end_points_;
    size_t local_index_;

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
    int lane_;
    size_t end_point_count_;
    size_t buffer_size_;
    std::vector<comms_bundle_t> submit_bundles_;
    std::vector<comms_bundle_t> release_bundles_;
    std::shared_ptr<moodycamel::ConcurrentQueue<comms_packet_t,CommsPacketTraits>> reap_queue_;
    moodycamel::ConcurrentQueue<comms_packet_t,CommsPacketTraits> catch_queue_;

    comms_accessor_t(comms_t *C,
                     int lane);
    void submit_n(comms_packet_t packet_list[],
                  size_t packet_count);
    size_t reap_n(comms_packet_t packet_list[],
                  size_t packet_count);
    size_t catch_n(comms_packet_t packet_list[],
                   size_t packet_count);
    void release_n(comms_packet_t packet_list[],
                   size_t packet_count);
} comms_accessor_t;

#endif // __COMMS_IMPL_H_
