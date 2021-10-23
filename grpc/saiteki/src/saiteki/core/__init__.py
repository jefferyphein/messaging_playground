from .grpc_server import GrpcServerBase
from .async_grpc_server import AsyncGrpcServerBase
from .async_service import launch_service
from .parameters import Parameter, Parameters, Constraint
from .optimization_context import OptimizationContext

def func_name(func):
    if isinstance(func, dict):
        return list(func.keys())[0]
    elif isinstance(func, str):
        return func
    else:
        raise ValueError(f"Unexpected function type {type(func)}. Expected 'dict' or 'str'.")

def load_func(func):
    if isinstance(func, dict):
        name = list(func.keys())[0]
        kwargs = func[name]
        func = load_func(name)
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
