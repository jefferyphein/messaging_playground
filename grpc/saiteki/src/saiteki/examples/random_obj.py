import random

class RandomObjective:
    def __init__(self, scalar):
        self.scalar = scalar

    def __call__(self, candidate):
        return self.scalar * random.random()
