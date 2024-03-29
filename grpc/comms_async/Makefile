CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf grpc` -std=c++17 -g -O3 -Wall -Werror

LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++`

# The protobuf compiler.
PROTOC = protoc

# The gRPC plugin to protoc.
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

# Path to the protobuf specifications.
PROTOS_PATH = ./protos

# Supply missing .proto files from here.
vpath %.proto $(PROTOS_PATH)

all: main

main: comms.pb.o comms.grpc.pb.o EndPoint.o Receiver.o Sender.o main.o comms.o
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

%.o: %.cc %.h SafeQueue.h
	$(CXX) -o $@ -c $< $(CPPFLAGS) $(LDFLAGS)

%.pb.o: %.pb.cc %.pb.h
	$(CXX) -o $@ -c $< $(CPPFLAGS) $(LDFLAGS)

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --cpp_out=. $<

.PRECIOUS: %.grpc.pb.cc
%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

clean:
	-rm -f *.o *.pb.cc *.pb.h main
