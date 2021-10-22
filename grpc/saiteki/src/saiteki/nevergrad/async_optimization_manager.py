import nevergrad as ng
from functools import partial
import saiteki.client

class AsyncOptimizationManager(saiteki.client.AsyncOptimizationManagerBase):
    async def optimize(self, budget, optimizer, *args, **kwargs):
        # Build nevergrad parameters from the saiteki parameters.
        ng_parameters = saiteki.nevergrad.Parameters(self.parameters)

        # Create the nevergrad optimizer.
        instrum = ng.p.Instrumentation(**ng_parameters)
        optimizer_class = ng.optimizers.registry[optimizer]
        optimizer = optimizer_class(
            parametrization=instrum,
            budget=budget,
        )

        # Suggest the starting candidate to the optimizer.
        optimizer.suggest(**self.parameters.candidate_dict())

        # Update the best candidate once nevergrad has been told the score.
        def tell_callback(opt, candidate, score):
            self.update_best_candidate(candidate.kwargs, score)

        # Register a callback whenever we tell nevergrad a score.
        optimizer.register_callback("tell", tell_callback)

        # Tell nevergrad the score of the candidate.
        def candidate_done_callback(candidate, task):
            score = task.result()
            optimizer.tell(candidate, score)

        # Submit all candidates under an optimization context to ensure all
        # optimization requests are completed before returning.
        async with saiteki.core.OptimizationContext() as context:
            for n in range(budget):
                candidate = optimizer.ask()
                task = await self.submit_candidate(candidate.kwargs, context)
                task.add_done_callback(partial(candidate_done_callback, candidate))
