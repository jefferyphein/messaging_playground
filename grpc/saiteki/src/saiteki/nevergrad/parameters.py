import collections
import nevergrad as ng


class Parameters(collections.abc.Mapping):
    def __init__(self, base_parameters):
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
        if parameter['type'] == "INT" or parameter['type'] == "INT64":
            return ng.p.Scalar(lower=parameter['lower'], upper=parameter['upper']).set_integer_casting()
        elif parameter['type'] == "FLOAT":
            return ng.p.Scalar(lower=parameter['lower'], upper=parameter['upper'])

    def __getitem__(self, key):
        return self.parameters[key]

    def __iter__(self):
        return iter(self.parameters)

    def __len__(self):
        return len(self.parameters)
