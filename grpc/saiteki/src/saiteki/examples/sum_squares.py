def sum_squares(candidate):
    return float(sum(value**2 for value in candidate.values()))

class SumSquares:
    def __init__(self, offset):
        self.offset = offset

    def __call__(self, candidate):
        return float(sum((value-self.offset)**2 for value in candidate.values()))
