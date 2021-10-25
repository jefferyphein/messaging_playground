"""Core functionality."""

from .grpc_server import GrpcServerBase  # noqa: F401
from .async_grpc_server import AsyncGrpcServerBase  # noqa: F401
from .parameters import Parameter, Parameters, Constraint  # noqa: F401
from .optimization_context import OptimizationContext  # noqa: F401
from .database import Database  # noqa: F401


def _func_name(func):
    if isinstance(func, dict):
        return list(func.keys())[0]
    elif isinstance(func, str):
        return func
    else:
        raise ValueError(f"Unexpected function type {type(func)}. Expected 'dict' or 'str'.")


def _load_func(func):
    if isinstance(func, dict):
        name = list(func.keys())[0]
        kwargs = func[name]
        func = _load_func(name)
        return func(**kwargs)
    elif isinstance(func, str):
        import importlib
        mod, name = func.split(":")
        module = importlib.import_module(mod)
        if hasattr(module, name):
            return getattr(module, name)
        else:
            raise RuntimeError(f"Unable to load function: {func}")
    else:
        raise ValueError(f"Unexpected function type {type(func)}. Expected 'dict' or 'str'.")
