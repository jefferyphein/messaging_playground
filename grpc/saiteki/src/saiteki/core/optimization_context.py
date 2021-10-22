import asyncio

class OptimizationContext:
    def __init__(self):
        self.lock = asyncio.Lock()

    async def __aenter__(self):
        self._done = False
        self.value = 0
        self.event = asyncio.Event()
        return self

    async def __aexit__(self, exc_t, exc_v, exc_tb):
        self._done = True
        await self.event.wait()
        return False

    async def update(self, step):
        async with self.lock:
            self.value += step
            if self._done and self.value == 0:
                self.event.set()
