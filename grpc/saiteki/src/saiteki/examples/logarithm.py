"""Logarithm objective function example."""

import math


class LogarithmObjective:
    """Logarithm callable object example."""

    def __init__(self, base=math.exp(1)):
        """Construct the callable objective function object.

        Arguments:
            base: The base of the logarithm.
        """
        if base <= 0.0:
            raise ValueError(f"Invalid base '{base}', must be positive")

        self.base = base

    def __call__(self, candidate, *args, **kwargs):
        """Objective function.

        Arguments:
            candidate: The candidate dictionary.

        Returns: The score.
        """
        return abs(math.log(1+sum(value**2 for value in candidate.values())) / math.log(self.base))
