import click
import asyncio
import saiteki
from .async_server import AsyncServer

from .. import saiteki_cli

@saiteki_cli.command('server')
@click.option("-n", "--num-workers", type=int, required=True, help="Number of workers")
@click.option("--keep-alive", is_flag=True, help="Keep server alive, even if remote shutdown requests are received.")
@click.option("--key", type=click.Path(exists=True), envvar="KEY", help="PEM private key")
@click.option("--cert", type=click.Path(exists=True), envvar="CERT", help="PEM certificate chain")
@click.option("--cacert", type=click.Path(exists=True), envvar="CACERT", help="Root certificate")
@click.option("--authentication", is_flag=True, help="Indicates that client authentication is required (only available when --cacert is provided)")
@click.argument("bind_addr", type=str)
@click.pass_context
def server_cli(ctx, *args, **kwargs):
    """Starts an optimization service."""

    exit(asyncio.run(saiteki.core.launch_service(AsyncServer, *args, **kwargs)))
