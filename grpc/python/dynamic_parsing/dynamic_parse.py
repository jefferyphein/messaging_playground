#!/usr/bin/env python

import sys
import argparse

import google.protobuf
import google.protobuf.descriptor
import google.protobuf.descriptor_pool
import google.protobuf.reflection
from google.protobuf import message as _message


parser = argparse.ArgumentParser()
parser.add_argument("descriptor")
parser.add_argument('proto')
parser.add_argument('message_type')

args = parser.parse_args()

pool = google.protobuf.descriptor_pool.DescriptorPool()

# Load the binary proto data from a file. This was generatred with
# `protoc -o hello.desc protos/hello.proto`
with open(args.descriptor, 'rb') as infile:
    serialized_pb = infile.read()
desc = google.protobuf.descriptor.FileDescriptor(
    name=args.proto,
    package=args.message_type.split('.')[0],
    syntax='proto3',
    serialized_pb=serialized_pb
)

# Now load the message type from the pool
msg_desc = desc.pool.FindMessageTypeByName(args.message_type)
Message = google.protobuf.reflection.GeneratedProtocolMessageType(
    args.message_type.split('.')[-1],
    (_message.Message,),
    {'DESCRIPTOR': msg_desc}
)

# Read data from stdin
data = Message.FromString(sys.stdin.buffer.read())
print(data)
