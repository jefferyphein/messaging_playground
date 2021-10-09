import asyncio
import uuid
import click
import signal
import logging
from functools import partial

from .. import discovery_cli
from .discovery_server import DiscoveryServer
from .cache import LocalCache

async def shutdown(loop, aio_server, signal=None):
    await aio_server.shutdown()
    logger = logging.getLogger("shutdown")
    if signal:
        logger.info(f"Received exit signal {signal.name}")
    logger.info("Shutting down server.")
    asyncio.run_coroutine_threadsafe(aio_server.shutdown(), loop)
    logger.info("Cancelling all tasks.")
    tasks = [ task for task in asyncio.all_tasks() if task is not asyncio.current_task() ]
    for task in tasks:
        print(task.get_name())
        task.cancel()
        await task
    logger.info(f"Cancelling {len(tasks)} outstanding task(s).")
    await asyncio.gather(*tasks, return_exceptions=True)
    loop.stop()

# TODO: This will not be called if the RPC functions throw exceptions. However,
# it should be possible to pass them through an interceptor so that exception
# get handled by this handler.
def handle_exception(aio_server, loop, context):
    logger = logging.getLogger("exception_handler")
    msg = context.get("exception", context["message"])
    logger.error(f"Caught exception {msg}")
    logger.info("Shutting down...")
    asyncio.create_task(shutdown(loop, aio_server))

async def _service(*args, **kwargs):
    aio_server = DiscoveryServer(*args, **kwargs)
    logger = logging.getLogger("discovery")

    loop = asyncio.get_event_loop()
    signals = [ signal.SIGTERM, signal.SIGINT, signal.SIGHUP ]
    for s in signals:
        loop.add_signal_handler(
            s, lambda s=s: asyncio.create_task(shutdown(loop, aio_server, signal=s))
        )
    loop.set_exception_handler(partial(handle_exception, aio_server))
    await aio_server.start()
    await aio_server.run_forever()

@discovery_cli.command('server')
@click.argument("bind_addr", type=str)
@click.option("--etcd-hostname", type=str, default="localhost")
@click.option("--etcd-port", type=int, default=2379)
@click.option("--etcd-lease-ttl", type=int, default=60, help="Etcd lease time-to-live (in seconds)")
@click.option("--etcd-lease-keep-alive", type=int, default=45, help="Etcd lease keep-alive interval (in seconds)")
@click.option("--etcd-namespace", type=str, default="/discovery", help="Etcd key namespace for storing updates")
@click.option("--sync-interval", type=float, default=120.0, help="How frequently (in seconds) to synchronize local cache with global cache")
@click.option("--service-name", type=str, default=str(uuid.uuid4()), help="Unique name for this discovery service")
@click.option("--key", type=click.File('rb'), default=None, help="Server key file")
@click.option("--cert", type=click.File('rb'), default=None, help="Server certification file")
@click.pass_context
def server_cli(ctx, *args, **kwargs):
    """Starts a discovery discovery service."""

    exit(asyncio.run(_service(*args, **kwargs)))
