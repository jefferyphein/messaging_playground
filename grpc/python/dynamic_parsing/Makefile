PROTOC_OPTIONS = --experimental_allow_proto3_optional

all: hello.desc protos/hello_pb2.py person.out

hello.desc: protos/hello.proto
	protoc -o $@ $< $(PROTOC_OPTIONS)

protos/hello_pb2.py: protos/hello.proto
	protoc -I./protos --python_out=./protos $< $(PROTOC_OPTIONS)

person.out: make_person.py
	python $< $@

ssh: dynamic_parse.py all
	python $< ssh://localhost$(PWD)/hello.desc sayhello.Person < person.out

sftp: dynamic_parse.py all
	python $< sftp://localhost$(PWD)/hello.desc sayhello.Person < person.out

file: dynamic_parse.py all
	python $< file://$(PWD)/hello.desc sayhello.Person < person.out

etcd: dynamic_parse.py etcd_put_key.py all
	python $< etcd:///protobuf/sayhello/Person sayhello.Person < person.out

etcd_put_key: etcd_put_key.py hello.desc
	python $^

clean:
	-rm -f hello.desc person.out protos/hello_pb2.py
