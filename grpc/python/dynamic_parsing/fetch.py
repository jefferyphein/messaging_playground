import time
from urllib.parse import urlparse

import paramiko
from scp import SCPClient

import google.protobuf
import google.protobuf.descriptor
import google.protobuf.descriptor_pb2
import google.protobuf.descriptor_pool
import google.protobuf.reflection
from google.protobuf import message as _message


class CachedDescriptor:
    def __init__(self, pool, message_type, descriptor_url):
        self.message_type = message_type
        serialized_pb = self._fetch_descriptor(descriptor_url)
        self._load_descriptor(pool, serialized_pb)
        self.creation_time = time.monotonic()
        self.pool = pool
        msg_desc = self.pool.FindMessageTypeByName(self.message_type)
        self._MessageClass = google.protobuf.reflection.GeneratedProtocolMessageType(
            self.message_type.split('.')[-1],
            (_message.Message,),
            {'DESCRIPTOR': msg_desc}
        )

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
        with SCPClient(ssh.get_transport()) as scp:
            with tempfile.NamedTemporaryFile() as tmp:
                scp.fetch(path, tmp)
                return tmp.read()


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

    @property
    def MessageClass(self):
        "Class corresponding to the given message type"
        return self._MessageClass


class DescriptorFetcher:
    def __init__(self, max_age):
        """
        Fetch protobuf descriptor files, with caching.

        max_age - Maximum age (in seconds) before a descriptor must be refetched
        """

        self._cache = {}
        self._descripter_fetch_time = {}
        self.pool = google.protobuf.descriptor_pool.DescriptorPool()
        self._max_age = max_age

    def fetch(self, url, message_type):
        "Retrieve the descriptor from the url and return the message"
        # First check how old the last fetch of that URL is
        last_fetch = self._descripter_fetch_time.get(url)
        force_update = False
        if last_fetch is None or time.monotonic() - last_fetch > self._max_age:
            # Either we've never gotten it before, or it's old so we
            # need to force and update for every message type coming
            # from that descriptor
            force_update = True

        descriptor = self._cache.get((url, message_type))

        if force_update or descriptor is None:
            print("Fetching update")
            descriptor = CachedDescriptor(self.pool, message_type, url)
            self._cache[(url, message_type)] = descriptor
            self._descripter_fetch_time[url] = time.monotonic()

        return descriptor.MessageClass
