import asyncio
import shlex
import logging
import os
import shutil
from functools import partial

class WatchProtocol(asyncio.SubprocessProtocol):
    def __init__(self, done_future, callbacks=tuple()):
        super().__init__()
        self._callbacks = tuple(callbacks)
        self._state = None
        self._done_future = done_future
        self._logger = logging.getLogger("WatchProtocol")

    def pipe_data_received(self, fd, data):
        state = data.decode().strip()
        if state and state != self._state:
            for f in self._callbacks:
                try:
                    f(self._state, state)
                except Exception as e:
                    self._logger.error("Caught exception in callback function: %s", e)

            self._state = state

    def process_exited(self):
        if not self._done_future.done():
            self._done_future.set_result(True)

class Watcher:
    def __init__(self):
        self._callbacks = list()
        self._done_future = None
        self._transport = None
        self._protocol = None
        self._logger = logging.getLogger("Watcher")

    def add_state_callback(self, f):
        self._callbacks.append(f)

    async def start(self, cmd, verify=tuple()):
        for filename in verify:
            if not os.path.exists(filename):
                raise FileNotFoundError("File '%s' does not exist." % filename)

        loop = asyncio.get_event_loop()

        self._done_future = asyncio.Future()
        self._transport, self._protocol = await loop.subprocess_exec(
            lambda: WatchProtocol(self._done_future, self._callbacks),
            *shlex.split(cmd),
        )

    def cancel(self):
        if self._transport is not None:
            self._transport.close()
            self._transport = None
            self._protocol = None

    async def done(self):
        await self._done_future
        self._done_future = None

async def main():
    sw = Watcher()

    done = asyncio.Future()
    def f(prev_state, new_state):
        print("%s -> %s" % (prev_state, new_state))
        if new_state == "DONE":
            done.set_result(True)
    sw.add_state_callback(f)

    await sw.start("stdbuf -oL ./main", verify=["./main", shutil.which("stdbuf")])
    await done
    sw.cancel()
    await sw.done()

if __name__ == "__main__":
    asyncio.run(main())
