"""The gRPC optimization servicer."""

import logging
import grpc
import json
import signal
import threading
import saiteki

LOGGER = logging.getLogger(__name__)


class SaitekiServicer(saiteki.protobuf.SaitekiServicer):
    """The gRPC optimization service."""

    def __init__(self, keep_alive=False):
        """Construct the optimization servicer.

        Arguments:
            keep_alive: Boolean indicating whether the server should stay alive
                even if a client requests a remote shutdown.
        """
        self.objective_functions = dict()
        self.lock = threading.Lock()
        self.keep_alive = keep_alive

    def ObjectiveFunction(self, request, context):
        """Objective function RPC.

        Arguments:
            request: A `saiteki.protobuf.CandidateRequest` object.
            context: A gRPC server context object.

        Returns: A `saiteki.protobuf.CandidateResponse` object.
        """
        try:
            # Load the objective function.
            obj_func = json.loads(request.objective_function_json)
            obj_func_name = saiteki.core._func_name(obj_func)

            # Get the objective function.
            with self.lock:
                objective_function = self.objective_functions.get(obj_func_name, None)
                if objective_function is None:
                    objective_function = saiteki.core._load_func(obj_func)
                    self.objective_functions[obj_func_name] = objective_function
                    LOGGER.debug("Added objective function to cache (func: %s)" % obj_func_name)
        except json.JSONDecodeError:
            context.set_code(grpc.StatusCode.INVALID_ARGUMENT)
            context.set_details("JSON decode error when parsing objective function.")
            LOGGER.exception("JSON decode error.")
            return saiteki.protobuf.CandidateResponse(score=float('inf'))
        except Exception as e:
            context.set_code(grpc.StatusCode.INVALID_ARGUMENT)
            context.set_details(f"Unexpected exception when acquiring objective function: {str(e)}")
            LOGGER.exception("Unexpected exception caught.")
            return saiteki.protobuf.CandidateResponse(score=float('inf'))

        # Convert candidate into a dictionary.
        candidate_dict = {
            parameter.name: getattr(parameter, parameter.WhichOneof('type'))
            for parameter in request.parameters
        }

        try:
            # Score the candidate.
            return saiteki.protobuf.CandidateResponse(
                score=objective_function(
                    candidate_dict,
                    timeout=context.time_remaining()
                )
            )
        except Exception as e:
            context.set_code(grpc.StatusCode.INVALID_ARGUMENT)
            context.set_details(f"Unexpected exception occurred while executing objective function: {str(e)}")
            return saiteki.protobuf.CandidateResponse(score=float('inf'))

    def Shutdown(self, request, context):
        """Shutdown RPC.

        Arguments:
            request: A `saiteki.protobuf.ShutdownRequest` object.
            context: A gRPC server context object.

        Returns: A `saiteki.protobuf.ShutdownResponse` object.
        """
        if not self.keep_alive:
            # Schedule shutdown upon rpc context completion. This ensures that
            # the client end receives a response before we issue a shutdown.
            def shutdown_cb():
                signal.raise_signal(signal.SIGTERM)
            context.add_callback(shutdown_cb)

        # Send shutdown response.
        return saiteki.protobuf.ShutdownResponse(ok=self.keep_alive)
