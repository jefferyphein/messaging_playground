all: saiteki_pb2_grpc.py
	sed -i -r 's/(^import .+_pb2.*)/from saiteki.protobuf \1/g' $<

%_pb2_grpc.py: %.proto
	python -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. $<
