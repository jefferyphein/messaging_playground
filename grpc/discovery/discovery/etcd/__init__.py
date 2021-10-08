from .client import EtcdClient
from .watch_manager import WatchManager
from .lease_manager import LeaseManager

def range_end(key):
    arr = bytearray(key)
    for i in reversed(range(len(key))):
        if arr[i] < 0xff:
            arr[i] += 1
            break
    return bytes(arr)
