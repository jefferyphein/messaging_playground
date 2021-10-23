import saiteki.core
from .async_optimization_manager_base import AsyncOptimizationManagerBase

class AsyncEvaluationManager(AsyncOptimizationManagerBase):
    async def optimize(self, budget, *args, **kwargs):
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
                if task is None: break
                task.add_done_callback(candidate_done_callback)

        return scores
