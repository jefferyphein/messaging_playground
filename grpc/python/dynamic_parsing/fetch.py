import time
import hashlib
from urllib.parse import urlparse
from urllib.request import urlopen
from dataclasses import dataclass

import paramiko
import etcd3

import google.protobuf
import google.protobuf.descriptor
import google.protobuf.descriptor_pb2
import google.protobuf.descriptor_pool
import google.protobuf.reflection
from google.protobuf import message as _message


class CachedDescriptor:
    def __init__(self, pool, descriptor_url, max_age, opener=None):
        """pool - gRPC descriptor pool

        descriptor_url -  the URL to fetch

        max_age - the amount of time in seconds before the cache entry is expired

        opener - URL opener function will be called with the URL as
        its only argument. Should return the serialized protobuf
        descriptor as `bytes`

        """
        serialized_pb = self._open(descriptor_url, opener)
        self._digest = hashlib.sha256(serialized_pb).hashdigest()
        self._load_descriptor(pool, serialized_pb)
        self.creation_time = time.monotonic()
        self.pool = pool
        self._is_expired = False
        self._max_age = max_age

        self._msg_classes = {}

    def digest(self):
        return self._digest


    def _open(self, url, opener=None):
        """Download the descriptor file from the URL"""
        scheme,netloc,path,params,query,fragment = urlparse(url)
        if opener is None:
            try:
                opener = getattr(self, '_fetch_%s'%scheme)
            except AttributeError:
                opener = self._fetch_unknown
        serialized_pb = opener(url)
        return serialized_pb

    @staticmethod
    def _fetch_file(url):
        scheme,netloc,path,params,query,fragment = urlparse(url)
        with open(path, 'rb') as infile:
            return infile.read()

    @staticmethod
    def _fetch_http(url):
        response = urlopen(url)
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

    # If the method is unknown, assume urllib is setup to handle it
    @staticmethod
    def _fetch_unknown(url):
        response = urlopen(url)
        return response.fp.read()

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

    def is_expired(self, digest=None):
        """True if the cache element has expired

        A cache entry is expired if it is either older than it's
        maximum age or the cached digest value does match the desired
        one

        """

        return self._is_expired or \
            (digest is not None and self.digest() != digest) or \
            (self.age() > self._max_age)

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

@dataclass
class CacheStats:
    hits: int = 0
    misses: int = 0

class DescriptorCache:
    def __init__(self, max_age):
        """
        Fetch protobuf descriptor files, with caching.

        max_age - Maximum age (in seconds) before a descriptor must be refetched
        """

        self._cache = {}
        self.pool = google.protobuf.descriptor_pool.DescriptorPool()
        self._max_age = max_age
        self.stats = CacheStats()

    def get(self, url, message_type, digest=None, opener=None):
        """Retrieve the descriptor from the url and return the message class

        url - URL path to the serialized proto descriptor

        message_type - fully qualified name of message type to return
        the class for

        opener - URL opener function will be called with the URL as
        its only argument. Should return the serialized protobuf
        descriptor as `bytes`

        digest - SHA256 hex digest of the desired descriptor. If this
        does not match the cached digest, the cache will be considered
        expired regardless of its age

        """
        # First check how old the last fetch of that URL is

        descriptor = self._cache.get(url)

        if descriptor is None or descriptor.is_expired():
            self.stats.misses += 1
            descriptor = CachedDescriptor(self.pool, url, self._max_age, opener)
            self._cache[url] = descriptor
        else:
            self.stats.hits += 1

        return descriptor.MessageClass(message_type)
