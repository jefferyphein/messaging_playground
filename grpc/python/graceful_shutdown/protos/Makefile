PROTOC_OPTIONS = --experimental_allow_proto3_optional

all: shutdown_pb2_grpc.py shutdown_pb2.py
	sed -i -r 's/^import (.+_pb2.*)/from . import \1/g' $<

shutdown_pb2.py: shutdown.proto
	protoc -I. --python_out=. $< $(PROTOC_OPTIONS)

shutdown_pb2_grpc.py: shutdown.proto
	python -m grpc_tools.protoc -I. --grpc_python_out=. $<

clean:
	-rm -f shutdown_pb2.py shutdown_pb2_grpc.py
