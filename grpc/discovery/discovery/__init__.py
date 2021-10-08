import click
import logging
import discovery.protobuf as protobuf

from sqlalchemy.orm import declarative_base
Base = declarative_base()

from .discovery import Discovery

@click.group()
@click.pass_context
def discovery_cli(ctx):
    logging.basicConfig(level=logging.INFO)

def _bootstrap(path):
    import pkgutil
    import importlib

    for _, name, is_pkg in pkgutil.iter_modules(path):
        if not is_pkg: continue
        try:
            importlib.import_module("%s.%s" % (__name__, name))
        except ModuleNotFoundError:
            pass

_bootstrap(__path__)
