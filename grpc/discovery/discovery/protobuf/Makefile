all: discovery_pb2_grpc.py etcd_pb2_grpc.py

%_pb2_grpc.py: %.proto
	python -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. $<
