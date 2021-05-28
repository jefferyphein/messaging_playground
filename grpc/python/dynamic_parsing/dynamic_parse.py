#!/usr/bin/env python

import sys
import argparse
import urllib.request
import io

import google.protobuf
import google.protobuf.descriptor
import google.protobuf.descriptor_pb2
import google.protobuf.descriptor_pool
import google.protobuf.reflection
from google.protobuf import message as _message

import etcd3

import fetch

class Etcd3Handler(urllib.request.BaseHandler):
    def etcd_open(self, req):
        etcd = etcd3.client(req.host)
        path = req.selector.strip('/')
        res,meta = etcd.get(path)
        data = io.BytesIO()
        data.write(res)
        data.seek(0)
        return urllib.request.addinfourl(data, meta, req.full_url)

opener = urllib.request.build_opener(Etcd3Handler())
urllib.request.install_opener(opener)

def parse_content_type(content_type):
    parts = [p.strip() for p in content_type.split(';')]
    content_type = parts[0]
    rest = parts[:1]
    args = {}
    for a in rest:
        key,val = a.split('=')
        args[key.strip()] = val.strip()
    return content_type, args


def main():
    parser = argparse.ArgumentParser(
        description="Read the given protobuf descriptor and parse binary data on stdin"
    )
    parser.add_argument("descriptor")
    parser.add_argument('message_type')

    args = parser.parse_args()

    pool = fetch.DescriptorCache(5*60)
    Message = pool.get(args.descriptor, args.message_type)

    # Read data from stdin
    data = Message.FromString(sys.stdin.buffer.read())
    print(data)
    return 0


if __name__ == '__main__':
    sys.exit(main())
