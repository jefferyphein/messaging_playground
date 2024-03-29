"""Asynchronous nevergrad optimizer."""

import nevergrad as ng
from functools import partial
from ..client import AsyncOptimizationClientBase
import saiteki


class AsyncOptimizationClient(AsyncOptimizationClientBase):
    """Asynchronous nevergrad optimizer."""

    async def optimize(self, budget, optimizer, *args, **kwargs):
        """Perform optimization with nevergrad.

        Arguments:
            budget: The number of evaluations to perform.
            optimizer: The name of the nevergrad optimizer to use.

        Returns: A (candidate, score)-tuple.
        """
        # Build nevergrad parameters from the saiteki parameters.
        ng_parameters = saiteki.nevergrad.Parameters(self.parameters)

        # Create the nevergrad optimizer.
        instrum = ng.p.Instrumentation(**ng_parameters)
        optimizer_class = ng.optimizers.registry[optimizer]
        options = dict(
            parametrization=instrum,
            budget=budget
        )
        if self._limit is not None:
            options['num_workers'] = self._limit
        optimizer = optimizer_class(**options)

        # Register cheap constraints on the optimizer.
        for constraint in ng_parameters.constraints:
            optimizer.parametrization.register_cheap_constraint(constraint)

        # Suggest the starting candidate to the optimizer.
        optimizer.suggest(**self.parameters.candidate_dict())

        # Update the best candidate once nevergrad has been told the score.
        def tell_callback(opt, candidate, score):
            self.update_best_candidate(candidate.kwargs, score)
            # Do something if the candidate was updated?

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
                if task is None:
                    break
                task.add_done_callback(partial(candidate_done_callback, candidate))

        candidate = optimizer.provide_recommendation()
        return candidate.kwargs, candidate.loss

    async def run(self, *args, **kwargs):
        """Run the optimizer.

        This function calls `self.optimize()` and then displays the best
        candidate and its score.
        """
        candidate, score = await self.optimize(*args, **kwargs)
        print(candidate, score)
