"""Optimization context."""

import asyncio


class OptimizationContext:
    """Optimization context."""

    def __init__(self):
        """Construct an optimization context object."""
        self.lock = asyncio.Lock()
        self._done = False
        self.value = 0
        self.event = asyncio.Event()

    async def __aenter__(self):
        """Enter asynchronous optimization context manager."""
        self._done = False
        self.value = 0
        self.event = asyncio.Event()
        return self

    async def __aexit__(self, exc_t, exc_v, exc_tb):
        """Exit asynchronous optimization context manager."""
        self._done = True
        async with self.lock:
            if self.value == 0:
                self.event.set()

        await self.event.wait()
        return False

    async def release(self):
        """Release an optimization context."""
        await self.update(-1)

    async def acquire(self):
        """Acquire an optimization context.

        This function indicates an optimization evaluation is being peformed.
        """
        await self.update(1)

    async def update(self, step):
        """Update context count.

        The function indicates an optimization evaluation has completed.
        """
        async with self.lock:
            self.value += step
            if self._done and self.value == 0:
                self.event.set()
