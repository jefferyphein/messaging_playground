import time
from urllib.parse import urlparse

import paramiko

import google.protobuf
import google.protobuf.descriptor
import google.protobuf.descriptor_pb2
import google.protobuf.descriptor_pool
import google.protobuf.reflection
from google.protobuf import message as _message


class CachedDescriptor:
    def __init__(self, pool, descriptor_url):
        serialized_pb = self._fetch_descriptor(descriptor_url)
        self._load_descriptor(pool, serialized_pb)
        self.creation_time = time.monotonic()
        self.pool = pool

        self._msg_classes = {}

    def _fetch_descriptor(self, url):
        """Download the descriptor file from the URL"""
        scheme,netloc,path,params,query,fragment = urlparse(url)
        try:
            fetcher = getattr(self, '_fetch_%s'%scheme)
        except AttributeError:
            raise ValueError("Don't know how to fetch '%s'"%scheme)
        serialized_pb = fetcher(url)
        return serialized_pb

    @staticmethod
    def _fetch_file(url):
        scheme,netloc,path,params,query,fragment = urlparse(url)
        with open(path, 'rb') as infile:
            return infile.read()

    @staticmethod
    def _fetch_http(url):
        response = urllib.request.urlopen(url)
        return response.read()

    @staticmethod
    def _fetch_ssh(url):
        scheme,netloc,path,params,query,fragment = urlparse(url)
        ssh = paramiko.SSHClient()
        ssh.load_system_host_keys()
        ssh.connect(netloc)
        with paramiko.sftp_client.SFTPClient.from_transport(ssh.get_transport()) as sftp:
            with sftp.open(path, 'r') as f:
                return f.read()

    @staticmethod
    def _fetch_sftp(url):
        return CachedDescriptor._fetch_ssh(url)

    @staticmethod
    def _load_descriptor(pool, serialized_pb):
        "Load the serialized protobuf data into the descriptor pool"
        file_desc = google.protobuf.descriptor_pb2.FileDescriptorSet.FromString(serialized_pb)

        for proto in file_desc.file:
            pool.Add(proto)
        return

    def age(self):
        "Age of the cached object in seconds"
        return time.monotonic() - self.creation_time

    def MessageClass(self, message_type):
        "Class corresponding to the given message type"
        try:
            MessageClass = self._msg_classes[message_type]
        except KeyError:
            msg_desc = self.pool.FindMessageTypeByName(message_type)
            MessageClass = google.protobuf.reflection.GeneratedProtocolMessageType(
            message_type.split('.')[-1],
            (_message.Message,),
            {'DESCRIPTOR': msg_desc}
            )
        return MessageClass



class DescriptorFetcher:
    def __init__(self, max_age):
        """
        Fetch protobuf descriptor files, with caching.

        max_age - Maximum age (in seconds) before a descriptor must be refetched
        """

        self._cache = {}
        self.pool = google.protobuf.descriptor_pool.DescriptorPool()
        self._max_age = max_age

    def fetch(self, url, message_type):
        "Retrieve the descriptor from the url and return the message"
        # First check how old the last fetch of that URL is

        descriptor = self._cache.get(url)

        if descriptor is None or descriptor.age() > self._max_age:
            print("Fetching update")
            descriptor = CachedDescriptor(self.pool, url)
            self._cache[url] = descriptor

        return descriptor.MessageClass(message_type)
