"""Asynchronous evaluation optimizer."""

import statistics
import saiteki.core
from .async_optimization_client_base import AsyncOptimizationClientBase


class AsyncEvaluationClient(AsyncOptimizationClientBase):
    """Asynchronmous evaluation optimizer."""

    async def optimize(self, budget, *args, **kwargs):
        """Evaluate the best-known candidate.

        Arguments:
            budget: The number of evaluations to perform.

        Returns: a list of scores, one for each evaluation performed.
        """
        candidate_dict = self.parameters.candidate_dict()
        scores = list()

        # Save the score into a list.
        def candidate_done_callback(task):
            score = task.result()
            scores.append(score)

        # Submit the same candidate under an optimization context to ensure all
        # evaluations are collected before returning.
        async with saiteki.core.OptimizationContext() as context:
            for n in range(budget):
                task = await self.submit_candidate(candidate_dict, context)
                if task is None:
                    break
                task.add_done_callback(candidate_done_callback)

        return scores

    async def run(self, *args, **kwargs):
        """Evaluate the best-known candidate and display statistics.

        This function calls `self.optimize` and then displays statistics about
        the distribution of scores it collected.
        """
        scores = await self.optimize(*args, **kwargs)
        print("samples", len(scores))
        print("mean", statistics.mean(scores))
        print("min", min(scores))
        print("max", max(scores))
        print("stdev", statistics.stdev(scores))
