import logging
import asyncio
import signal
import saiteki

LOGGER = logging.getLogger(__name__)

class AsyncSaitekiServicer(saiteki.protobuf.SaitekiServicer):
    def __init__(self):
        pass

    def ObjectiveFunction(self, request, context):
        candidate_dict = {
            parameter.name: getattr(parameter, parameter.WhichOneof('type'))
                for parameter in request.parameters
        }
        score = sum(value**2 for value in candidate_dict.values())

        return saiteki.protobuf.CandidateResponse(score=score)

    async def Shutdown(self, request, context):
        signal.raise_signal(signal.SIGTERM)
        return saiteki.protobuf.ShutdownResponse()
