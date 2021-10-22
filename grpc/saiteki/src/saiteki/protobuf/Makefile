PROTOC = protoc
GRPC_PYTHON_PLUGIN = grpc_python_plugin
GRPC_PYTHON_PLUGIN_PATH ?= `which $(GRPC_PYTHON_PLUGIN)`

all: saiteki_pb2.py saiteki_pb2_grpc.py

.PRECIOUS: %_pb2_grpc.py
%_pb2_grpc.py: %.proto
	protoc -I. --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_PYTHON_PLUGIN_PATH) $<
	sed -i -r 's/(^import .+_pb2.*)/from saiteki.protobuf \1/g' $@

.PRECIOUS: %_pb2.py
%_pb2.py: %.proto
	protoc -I. --python_out=. $<

clean:
	-rm -f saiteki_pb2*
