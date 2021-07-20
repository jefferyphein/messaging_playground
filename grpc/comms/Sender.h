#pragma once

#include <grpcpp/grpcpp.h>

extern "C" {
#include "comms.h"
}

#include "comms.grpc.pb.h"

class Sender {
public:
    Sender() = delete;
    Sender(comms_end_point_t *end_point);

    void send(char *process_name);

private:
    std::unique_ptr<comms::Comms::Stub> stub_;
    std::string name_;
    std::string address_;
};
