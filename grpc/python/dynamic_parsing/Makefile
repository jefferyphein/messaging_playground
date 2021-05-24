PROTOC_OPTIONS = --experimental_allow_proto3_optional
ETCD_KEY = /protobuf/sayhello/Person
GRPC_MESSAGE_TYPE = sayhello.Person

all: hello.desc protos/hello_pb2.py person.out

hello.desc: protos/hello.proto
	protoc -o $@ $< $(PROTOC_OPTIONS)

protos/hello_pb2.py: protos/hello.proto
	protoc -I./protos --python_out=./protos $< $(PROTOC_OPTIONS)

person.out: make_person.py
	python $< $@

ssh: dynamic_parse.py all
	python $< ssh://localhost$(PWD)/hello.desc $(GRPC_MESSAGE_TYPE) < person.out

sftp: dynamic_parse.py all
	python $< sftp://localhost$(PWD)/hello.desc $(GRPC_MESSAGE_TYPE) < person.out

file: dynamic_parse.py all
	python $< file://$(PWD)/hello.desc $(GRPC_MESSAGE_TYPE) < person.out

etcd: dynamic_parse.py etcd_put_key all
	python $< etcd://$(ETCD_KEY) $(GRPC_MESSAGE_TYPE) < person.out

etcd_put_key: etcd_put_key.py hello.desc
	python $^ $(ETCD_KEY)

clean:
	-rm -f hello.desc person.out protos/hello_pb2.py
