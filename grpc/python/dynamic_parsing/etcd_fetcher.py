import etcd3
from urllib.parse import urlparse

class ETCDFetcher:
    def __init__(self):
        self._client = etcd3.client()

    def __call__(self, url):
        scheme,netloc,path,params,query,fragment = urlparse(url)
        serialized_pb, _ = self._client.get(path)
        return serialized_pb
