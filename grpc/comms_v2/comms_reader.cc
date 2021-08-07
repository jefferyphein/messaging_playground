#include <sstream>

extern "C" {
#include "comms.h"
}
#include "comms_impl.h"

comms_reader_t::comms_reader_t(comms_t *C, const char *address)
        : C_(C)
        , address_(address)
        , service_()
        , server_(nullptr)
        , server_built_(false)
{}

int comms_reader_create(comms_reader_t **R, comms_t *C, const char *address, char **error) {
    try {
        R[0] = new comms_reader_t(C, address);
        return 0;
    }
    catch (std::bad_alloc& e) {
        std::stringstream ss;
        ss << "Unable to allocate comms reader";
        comms_set_error(error, ss.str().c_str());
        return 1;
    }
}

int comms_reader_start(comms_reader_t *R, char **error) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(R->address_, grpc::InsecureServerCredentials());
    builder.RegisterService(&R->service_);
    R->server_ = builder.BuildAndStart();
    R->server_built_ = true;

    if (R->server_ == nullptr) {
        std::stringstream ss;
        ss << "Unable to start server at " << R->address_;
        comms_set_error(error, ss.str().c_str());
        return 1;
    }

    R->server_->Wait();

    return 0;
}

int comms_reader_destroy(comms_reader_t *R, char **error) {
    return 0;
}

grpc::Status ReaderServiceImpl::Send(grpc::ServerContext *context,
                                     const comms::Packets *request,
                                     comms::PacketResponse *response) {
    return grpc::Status::OK;
}
