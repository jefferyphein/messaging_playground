import etcd3
import sys

if len(sys.argv) < 3:
    print("Usage: %s descriptor etcd_key" % sys.argv[0])
    sys.exit(1)

desc = sys.argv[1]
with open(desc, 'rb') as f:
    data = f.read()

client = etcd3.client()
client.put(sys.argvp[2], data)
