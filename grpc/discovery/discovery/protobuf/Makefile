PROTOC = protoc
GRPC_PYTHON_PLUGIN = grpc_python_plugin
GRPC_PYTHON_PLUGIN_PATH ?= `which $(GRPC_PYTHON_PLUGIN)`

all: discovery_pb2_grpc.py discovery_pb2.py etcd_pb2_grpc.py etcd_pb2.py

.PRECIOUS: %._pb2_grpc.py
%_pb2_grpc.py: %.proto
	$(PROTOC) -I. --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_PYTHON_PLUGIN_PATH) $<
	sed -i -r 's/(^import .+_pb2.*)/from discovery.protobuf \1/g' $@

.PRECIOUS: %_pb2.py
%_pb2.py: %.proto
	$(PROTOC) -I. --python_out=. $<

clean:
	-rm -f discovery_pb2* etcd_pb2*
