#!/usr/bin/env python

import sys
import argparse

import google.protobuf
import google.protobuf.descriptor
import google.protobuf.descriptor_pb2
import google.protobuf.descriptor_pool
import google.protobuf.reflection
from google.protobuf import message as _message

import fetch

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

    pool = fetch.DescriptorFetcher(5*60)
    Message = pool.fetch(args.descriptor, args.message_type)

    # Read data from stdin
    data = Message.FromString(sys.stdin.buffer.read())
    print(data)
    return 0


if __name__ == '__main__':
    sys.exit(main())
