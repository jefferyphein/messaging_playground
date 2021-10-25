"""Sum of squares objective function example."""


def sum_squares(candidate, *args, **kwargs):
    """Sum of square function example.

    Arguments:
        candidate: The candidate dictionary.

    Returns: Sum of squares of all input values.
    """
    return float(sum(value**2 for value in candidate.values()))


class SumSquares:
    """Sum of squares callable class example."""

    def __init__(self, offset):
        """Construct the callable objective function object.

        Arguments:
            offset: The offset for computing the sum of squares.
        """
        self.offset = offset

    def __call__(self, candidate, *args, **kwargs):
        """Objective function.

        Arguments:
            candidate: The candidate dictionary.

        Returns: Sum of squares of the candidate parameters.
        """
        return float(sum((value-self.offset)**2 for value in candidate.values()))
