import asyncio
import uuid
import click
import signal
import logging
from functools import partial

from .. import discovery_cli
from .discovery_server import DiscoveryServer
from .cache import LocalCache

async def _service(*args, **kwargs):
    aio_server = DiscoveryServer(*args, **kwargs)
    logger = logging.getLogger("discovery")

    def handler(loop, signum, frame):
        logger.critical("Signal '%s' (code: %s) received, shutting down service.", signal.strsignal(signum), signum)
        asyncio.run_coroutine_threadsafe(aio_server.shutdown(), loop)

    signal.signal(signal.SIGTERM, partial(handler, asyncio.get_event_loop()))
    signal.signal(signal.SIGINT, partial(handler, asyncio.get_event_loop()))

    await aio_server.start()
    await aio_server.run_forever()

@discovery_cli.command('server')
@click.argument("bind_addr", type=str)
@click.option("--etcd-hostname", type=str, default="localhost")
@click.option("--etcd-port", type=int, default=2379)
@click.option("--etcd-lease-ttl", type=int, default=60, help="Etcd lease time-to-live (in seconds)")
@click.option("--etcd-lease-keep-alive", type=int, default=45, help="Etcd lease keep-alive interval (in seconds)")
@click.option("--etcd-watch-prefix", type=str, default="/discovery", help="Etcd key namespace to watch for updates")
@click.option("--service-name", type=str, default=str(uuid.uuid4()), help="Unique name for this discovery service")
@click.option("--key", type=click.File('rb'), default=None, help="Server key file")
@click.option("--cert", type=click.File('rb'), default=None, help="Server certification file")
@click.pass_context
def server_cli(ctx, *args, **kwargs):
    """Starts a discovery discovery service."""

    exit(asyncio.run(_service(*args, **kwargs)))
