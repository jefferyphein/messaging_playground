import click
import signal
import yaml
import asyncio
import tempfile
import nevergrad as ng
import os
import logging
from functools import partial
import saiteki

from .. import saiteki_cli
from saiteki.client.async_evaluation_client import AsyncEvaluationClient

LOGGER = logging.getLogger(__name__)


async def _shutdown(loop, aio_server, aio_client, signal=None):
    LOGGER.critical("Shutdown signal received (%s), waiting for server shutdown...", signal)
    await aio_client.shutdown()
    await aio_server.shutdown()


def _handle_exception(aio_server, aio_client, loop, context):
    LOGGER.exception("An uncaught exception was detected")
    asyncio.create_task(_shutdown(loop, aio_server, aio_client))


async def _standalone(parameters, evaluation, *args, **kwargs):
    aio_server = saiteki.server.AsyncServer(*args, **kwargs)
    loop = asyncio.get_event_loop()

    # Create the asyncio client.
    if evaluation:
        aio_client = AsyncEvaluationClient(parameters, *args, **kwargs)
    else:
        aio_client = saiteki.nevergrad.AsyncOptimizationClient(parameters, *args, **kwargs)

    # Add signal handlers and exception handler to main event loop.
    signals = [signal.SIGTERM, signal.SIGINT, signal.SIGHUP]
    for s in signals:
        loop.add_signal_handler(
            s, lambda s=s: asyncio.create_task(_shutdown(loop, aio_server, aio_client, signal=s))
        )
    loop.set_exception_handler(partial(_handle_exception, aio_server, aio_client))

    # Start server.
    await aio_server.start()

    # Run the client.
    await aio_client.run(*args, **kwargs)

    # Shutdown the client, then the server.
    await aio_client.shutdown()
    await aio_server.shutdown()

    # Clean up all loose ends and stop the loop.
    tasks = list(task for task in asyncio.all_tasks() if task is not asyncio.current_task())
    LOGGER.debug("Server shutdown, cancelling %d outstanding task(s)...", len(tasks))
    for task in tasks:
        task.cancel()
    await asyncio.gather(*tasks, return_exceptions=True)
    LOGGER.debug("All tasks cancelled. Goodbye.")
    loop.stop()


@saiteki_cli.command('standalone')
@click.option("-n", "--num-workers", type=int, required=True,
              help="Number of workers")
@click.option("--budget", type=int, required=True,
              help="Optimization budget (number of optimization attempts)")
@click.option("--evaluation", is_flag=True,
              help="Run in evaluation mode. Displays statistics upon completion.")
@click.option("--limit", type=int, required=False, default=0,
              help="Limits the number of simultaneous optimizations (<=0 indicates no limit, default: 0)",)
@click.option("--deadline", type=float, required=False, default=0.0,
              help="Deadline for each optimization request (<=0 indicates no deadline, default: 0)")
@click.option("--threshold", type=float, required=False, default=0.0,
              help="Stop optimization once threshold is reached (<=0 indicates no threshold, default: 0)")
@click.option("--optimizer", type=click.Choice(sorted(ng.optimizers.registry.keys())), required=True, default="NGOpt",
              help="Optimizer name")
@click.argument("parameters", type=click.File())
@click.pass_context
def server_cli(ctx, parameters, *args, **kwargs):
    """Starts a standalone optimization service."""

    data = yaml.safe_load(parameters.read())
    parameters.close()
    parameters = saiteki.core.Parameters(**data)

    with tempfile.TemporaryDirectory() as socket_dir:
        # Specify the socket as the bind address for the server, and set it as
        # a singleton remote host list for the client.
        kwargs['bind_addr'] = "unix://"+os.path.join(socket_dir, "saiteki.sck")
        kwargs['remote_hosts'] = [kwargs['bind_addr']]
        exit(asyncio.run(_standalone(parameters, *args, **kwargs)))
