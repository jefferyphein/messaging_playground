CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf grpc libuv` -std=c++11 -g -O3 -fPIC
LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++ googleapis_cpp_api_annotations_protos libuv`

PROTOC = protoc

GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

PROTOS_PATH = ./protos

vpath %.proto $(PROTOS_PATH)

all: main

main: main.cc libetcd.so
	$(CXX) -o $@ $< -letcd -L. $(CPPFLAGS) $(LDFLAGS)

libetcd.so: etcd.pb.o etcd.grpc.pb.o Client.o Future.o Response.o Value.o Watch.o
	$(CXX) -shared -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

%.o: %.cc libetcd.h
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
	-rm -f main *.o *.so *.pb.cc *.pb.h
