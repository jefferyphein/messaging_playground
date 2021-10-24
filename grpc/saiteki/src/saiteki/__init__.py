import click
import logging

LOGGING_LEVELS = dict(
    debug=logging.DEBUG,
    info=logging.INFO,
    warning=logging.WARNING,
    error=logging.ERROR,
    critical=logging.CRITICAL,
)


@click.group()
@click.option("--loglevel", type=click.Choice(LOGGING_LEVELS.keys()), default='info',
              help="Logging level")
@click.pass_context
def saiteki_cli(ctx, loglevel):
    logger = logging.getLogger()
    logger.setLevel(LOGGING_LEVELS[loglevel])


def _bootstrap(path):
    import pkgutil
    import importlib

    for _, name, is_pkg in pkgutil.iter_modules(path):
        if not is_pkg:
            continue
        try:
            importlib.import_module("%s.%s" % (__name__, name))
        except ImportError:
            pass


_bootstrap(__path__)
