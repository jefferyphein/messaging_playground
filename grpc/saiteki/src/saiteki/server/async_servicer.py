import logging
import grpc
import json
import signal
import threading
import saiteki

LOGGER = logging.getLogger(__name__)


class SaitekiServicer(saiteki.protobuf.SaitekiServicer):
    def __init__(self, keep_alive=False):
        self.objective_functions = dict()
        self.lock = threading.Lock()
        self.keep_alive = keep_alive

    def ObjectiveFunction(self, request, context):
        try:
            # Load the objective function.
            obj_func = json.loads(request.objective_function_json)
            obj_func_name = saiteki.core.func_name(obj_func)

            # Get the objective function.
            with self.lock:
                objective_function = self.objective_functions.get(obj_func_name, None)
                if objective_function is None:
                    objective_function = saiteki.core.load_func(obj_func)
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
        print(candidate_dict)

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
        if not self.keep_alive:
            # Schedule shutdown upon rpc context completion. This ensures that
            # the client end receives a response before we issue a shutdown.
            def shutdown_cb():
                signal.raise_signal(signal.SIGTERM)
            context.add_callback(shutdown_cb)

        # Send shutdown response.
        return saiteki.protobuf.ShutdownResponse(ok=self.keep_alive)
