import math

class LogarithmObjective:
    def __init__(self, base=math.exp(1)):
        if base <= 0.0:
            raise ValueError(f"Invalid base '{base}', must be positive")

        self.base = base

    def __call__(self, candidate, *args, **kwargs):
        return abs(math.log(1+sum(value**2 for value in candidate.values())) / math.log(self.base))
