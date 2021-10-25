"""Random objective function example."""

import random


def pure_random(candidate, *args, **kwargs):
    """Random number function example.

    Arguments:
        candidate: The candidate dictionary.

    Returns: A random number in range [0.0, 1.0].
    """
    return random.random()


class RandomObjective:
    """Random number callable object example."""

    def __init__(self, scalar):
        """Construct the callable objective function object.

        Arguments:
            scalar: A nonnegative scalar to control the range of the randomness.
        """
        self.scalar = scalar

    def __call__(self, candidate, *args, **kwargs):
        """Objective function.

        Arguments:
            candidate: The candidate dictionary.

        Returns: A random number in range [0.0, scalar].
        """
        return self.scalar * random.random()
