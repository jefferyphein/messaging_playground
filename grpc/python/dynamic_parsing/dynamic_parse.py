#!/usr/bin/env python

import sys
import argparse

import google.protobuf
import google.protobuf.descriptor
import google.protobuf.descriptor_pb2
import google.protobuf.descriptor_pool
import google.protobuf.reflection
from google.protobuf import message as _message


def load_descriptor(pool, desc_file):
    """Load the binary proto data from a file into a descriptor pool.

    This loads data as generatred by e.g.
    `protoc -o hello.desc protos/hello.proto`

    """
    with open(desc_file, 'rb') as infile:
        file_desc = google.protobuf.descriptor_pb2.FileDescriptorSet.FromString(infile.read())

    for proto in file_desc.file:
        pool.Add(proto)
    return


def get_message_class(pool, message_type):
    """Get the message class from the pool

    message type is of the form 'package.Message'

    """
    msg_desc = pool.FindMessageTypeByName(message_type)
    Message = google.protobuf.reflection.GeneratedProtocolMessageType(
        message_type.split('.')[-1],
        (_message.Message,),
        {'DESCRIPTOR': msg_desc}
    )
    return Message


def main():
    parser = argparse.ArgumentParser(
        description="Read the given protobuf descriptor and parse binary data on stdin"
    )
    parser.add_argument("descriptor")
    parser.add_argument('message_type')

    args = parser.parse_args()

    pool = google.protobuf.descriptor_pool.DescriptorPool()
    load_descriptor(pool, args.descriptor)

    Message = get_message_class(pool, args.message_type)

    # Read data from stdin
    data = Message.FromString(sys.stdin.buffer.read())
    print(data)
    return 0


if __name__ == '__main__':
    sys.exit(main())
