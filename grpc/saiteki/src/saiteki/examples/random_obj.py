import random


def pure_random(candidate, *args, **kwargs):
    return random.random()


class RandomObjective:
    def __init__(self, scalar):
        self.scalar = scalar

    def __call__(self, candidate, *args, **kwargs):
        return self.scalar * random.random()
