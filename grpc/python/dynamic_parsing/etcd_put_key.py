import etcd3
import sys

if len(sys.argv) < 2:
    print("Must provide filename as the first argument.")
    sys.exit(1)

desc = sys.argv[1]
with open(desc, 'rb') as f:
    data = f.read()

client = etcd3.client()
client.put("/protobuf/sayhello/Person", data)
