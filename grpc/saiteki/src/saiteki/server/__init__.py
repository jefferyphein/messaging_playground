import click
import signal
import logging
import asyncio
from functools import partial
from .async_server import AsyncServer

from .. import saiteki_cli

LOGGER = logging.getLogger(__name__)


async def _shutdown(loop, aio_server, signal=None):
    LOGGER.critical("Shutdown signal received (%s), waiting for server shutdown...", signal)
    await aio_server.shutdown()


def _handle_exception(aio_server, loop, context):
    LOGGER.exception("An uncaught exception was detected")
    asyncio.create_task(_shutdown(loop, aio_server))


async def _server(*args, **kwargs):
    aio_server = AsyncServer(*args, **kwargs)
    loop = asyncio.get_event_loop()

    # Add signal handlers and exception handler to main event loop.
    signals = [signal.SIGTERM, signal.SIGINT, signal.SIGHUP]
    for s in signals:
        loop.add_signal_handler(
            s, lambda s=s: asyncio.create_task(_shutdown(loop, aio_server, signal=s))
        )
    loop.set_exception_handler(partial(_handle_exception, aio_server))

    # Start server and wait for shutdown.
    await aio_server.start()
    await aio_server.run_forever()

    # Clean up all loose ends and stop the loop.
    tasks = list(task for task in asyncio.all_tasks() if task is not asyncio.current_task())
    LOGGER.debug("Server shutdown, cancelling %d outstanding task(s)...", len(tasks))
    for task in tasks:
        task.cancel()
    await asyncio.gather(*tasks, return_exceptions=True)
    LOGGER.debug("All tasks cancelled. Goodbye.")
    loop.stop()


@saiteki_cli.command('server')
@click.option("-n", "--num-workers", type=int, required=True,
              help="Number of workers")
@click.option("--keep-alive", is_flag=True,
              help="Keep server alive, even if remote shutdown requests are received.")
@click.option("--key", type=click.Path(exists=True), envvar="KEY",
              help="PEM private key")
@click.option("--cert", type=click.Path(exists=True), envvar="CERT",
              help="PEM certificate chain")
@click.option("--cacert", type=click.Path(exists=True), envvar="CACERT",
              help="Root certificate")
@click.option("--authentication", is_flag=True,
              help="Indicates that client authentication is required (only available when --cacert is provided)")
@click.argument("bind_addr", type=str)
@click.pass_context
def server_cli(ctx, *args, **kwargs):
    """Starts an optimization service."""

    exit(asyncio.run(_server(*args, **kwargs)))
