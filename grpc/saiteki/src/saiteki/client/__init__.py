import click
import grpc
import yaml
import asyncio
import nevergrad as ng
import saiteki

from .. import saiteki_cli
from .async_optimization_manager_base import AsyncOptimizationManagerBase
from .async_evaluation_manager import AsyncEvaluationManager
from .remote_host import RemoteHost

async def optimizer(parameters, evaluation, *args, **kwargs):
    data = yaml.safe_load(parameters.read())
    parameters.close()
    parameters = saiteki.core.Parameters(**data)

    if not evaluation:
        manager = saiteki.nevergrad.AsyncOptimizationManager(parameters, *args, **kwargs)
        candidate, score = await manager.optimize(*args, **kwargs)
        print(candidate, score)
    else:
        manager = AsyncEvaluationManager(parameters, *args, **kwargs)
        scores = await manager.optimize(*args, **kwargs)

        import statistics
        print("samples", len(scores))
        print("mean", statistics.mean(scores))
        print("min", min(scores))
        print("max", max(scores))
        print("stdev", statistics.stdev(scores))

@saiteki_cli.command('client')
@click.option("--budget", type=int, required=True, help="Optimization budget (number of optimization attempts)")
@click.option("--limit", type=int, required=False, help="Limits the number of simultaneous optimizations (<=0 indicates no limit, default: 0)", default=0)
@click.option("--deadline", type=float, required=False, help="Deadline for each optimization request (<=0 indicates no deadline, default: 0)", default=0.0)
@click.option("--threshold", type=float, required=False, help="Stop optimization once threshold is reached (<=0 indicates no threshold, default: 0)", default=0.0)
@click.option("--optimizer", type=click.Choice(sorted(ng.optimizers.registry.keys())), required=True, help="Optimizer name", default="NGOpt")
@click.option("--evaluation", is_flag=True, help="Run in evaluation mode. Displays statistics upon completion.")
@click.option("--key", type=click.Path(exists=True), envvar="KEY", help="PEM private key")
@click.option("--cert", type=click.Path(exists=True), envvar="CERT", help="PEM certificate chain")
@click.option("--cacert", type=click.Path(exists=True), envvar="CACERT", help="Root certificate")
@click.argument("parameters", type=click.File())
@click.argument("hosts", nargs=-1)
@click.pass_context
def client_cli(ctx, *args, **kwargs):
    """Starts an optimization client."""

    exit(asyncio.run(optimizer(*args, **kwargs)))
