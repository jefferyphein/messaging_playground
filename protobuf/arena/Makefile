CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf` -std=c++11 -g -O3 -fPIC

LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf`

# The protobuf compiler.
PROTOC = protoc

# The gRPC plugin to protoc.
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

# Path to the protobuf specifications.
PROTOS_PATH = ./protos

# Supply missing .proto files from here.
vpath %.proto $(PROTOS_PATH)

all: arena

arena: arena.cc arena.pb.o
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

%.o: %.cc
	$(CXX) -o $@ -c $< $(CPPFLAGS) $(LDFLAGS)

%.pb.o: %.pb.cc %.pb.h
	$(CXX) -o $@ -c $< $(CPPFLAGS) $(LDFLAGS)

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --cpp_out=. $<

clean:
	-rm -f *.o *.pb.cc *.pb.h arena
