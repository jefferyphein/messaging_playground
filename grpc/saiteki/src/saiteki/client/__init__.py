import click
import yaml
import asyncio
import nevergrad as ng
import signal
import logging
from functools import partial

import saiteki
from .. import saiteki_cli
from .async_optimization_client_base import AsyncOptimizationClientBase  # noqa: F401
from .async_evaluation_client import AsyncEvaluationClient
from .remote_host import RemoteHost  # noqa: F401

LOGGER = logging.getLogger(__name__)


async def _shutdown(loop, client, signal=None):
    LOGGER.critical("Shutdown signal received (%s), waiting for server shutdown...", signal)
    await client.shutdown()


def _handle_exception(client, loop, context):
    LOGGER.exception("An uncaught exception was detected")
    asyncio.create_task(_shutdown(loop, client))


async def optimizer(parameters, evaluation, *args, **kwargs):
    loop = asyncio.get_event_loop()

    # Set up the client.
    if evaluation:
        client = AsyncEvaluationClient(parameters, *args, **kwargs)
    else:
        client = saiteki.nevergrad.AsyncOptimizationClient(parameters, *args, **kwargs)

    # Add signal handlers and exception handler to main event loop.
    signals = [signal.SIGTERM, signal.SIGINT, signal.SIGHUP]
    for s in signals:
        loop.add_signal_handler(
            s, lambda s=s: asyncio.create_task(_shutdown(loop, client, signal=s))
        )
    loop.set_exception_handler(partial(_handle_exception, client))

    # Run the optimizer.
    if evaluation:
        scores = await client.optimize(*args, **kwargs)

        import statistics
        print("samples", len(scores))
        print("mean", statistics.mean(scores))
        print("min", min(scores))
        print("max", max(scores))
        print("stdev", statistics.stdev(scores))
    else:
        candidate, score = await client.optimize(*args, **kwargs)
        print(candidate, score)

    # Shutdown client.
    await client.shutdown()

    # Clean up all loose ends and stop the loop.
    tasks = list(task for task in asyncio.all_tasks() if task is not asyncio.current_task())
    LOGGER.debug("Server shutdown, cancelling %d outstanding task(s)...", len(tasks))
    for task in tasks:
        task.cancel()
    await asyncio.gather(*tasks, return_exceptions=True)
    LOGGER.debug("All tasks cancelled. Goodbye.")
    loop.stop()


@saiteki_cli.command('client')
@click.option("--budget", type=int, required=True,
              help="Optimization budget (number of optimization attempts)")
@click.option("--limit", type=int, required=False, default=0,
              help="Limits the number of simultaneous optimizations (<=0 indicates no limit, default: 0)",)
@click.option("--deadline", type=float, required=False, default=0.0,
              help="Deadline for each optimization request (<=0 indicates no deadline, default: 0)")
@click.option("--threshold", type=float, required=False, default=0.0,
              help="Stop optimization once threshold is reached (<=0 indicates no threshold, default: 0)")
@click.option("--optimizer", type=click.Choice(sorted(ng.optimizers.registry.keys())), required=True, default="NGOpt",
              help="Optimizer name")
@click.option("--evaluation", is_flag=True,
              help="Run in evaluation mode. Displays statistics upon completion.")
@click.option("--shutdown-remote-hosts", is_flag=True,
              help="Shutdown remote hosts when optimization finishes.")
@click.option("--key", type=click.Path(exists=True), envvar="KEY",
              help="PEM private key")
@click.option("--cert", type=click.Path(exists=True), envvar="CERT",
              help="PEM certificate chain")
@click.option("--cacert", type=click.Path(exists=True), envvar="CACERT",
              help="Root certificate")
@click.argument("parameters", type=click.File())
@click.argument("hosts", nargs=-1)
@click.pass_context
def client_cli(ctx, parameters, *args, **kwargs):
    """Starts an optimization client."""

    data = yaml.safe_load(parameters.read())
    parameters.close()
    parameters = saiteki.core.Parameters(**data)

    exit(asyncio.run(optimizer(parameters, *args, **kwargs)))
