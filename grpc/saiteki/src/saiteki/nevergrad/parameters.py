"""Nevergrad parameters."""

import collections
import nevergrad as ng


class Parameters(collections.abc.Mapping):
    """Container for the nevergrad optimization parametrization."""

    def __init__(self, base_parameters):
        """Convert `saiteki.core.Parameters` into nevergrad parameters.

        Arguments:
            base_parameters: A `saiteki.core.Parameters` object.
        """
        super().__init__()
        self.parameters = {
            parameter['name']: self.scalar(parameter) for parameter in base_parameters.values()
        }

        def func(constraint):
            def constraint_func(x):
                return constraint(x[1])
            return constraint_func

        self.constraints = list(func(constraint) for constraint in base_parameters.constraints)

    def scalar(self, parameter):
        """Convert a `saiteki.core.Parameter` object into a nevergrad scalar.

        Arguments:
            parameter: A `saiteki.core.Parameter` object.

        Returns: A `nevergrad.p.Parameter` scalar object.
        """
        if parameter['type'] == "INT" or parameter['type'] == "INT64":
            return ng.p.Scalar(lower=parameter['lower'], upper=parameter['upper']).set_integer_casting()
        elif parameter['type'] == "FLOAT":
            return ng.p.Scalar(lower=parameter['lower'], upper=parameter['upper'])

    def __getitem__(self, key):
        """Get item."""
        return self.parameters[key]

    def __iter__(self):
        """Return the iterator for all parameters."""
        return iter(self.parameters)

    def __len__(self):
        """Return the number of parameters."""
        return len(self.parameters)
