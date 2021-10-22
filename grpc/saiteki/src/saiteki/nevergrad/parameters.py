import collections
import nevergrad as ng
import saiteki.core

class Parameters(collections.abc.Mapping):
    def __init__(self, base_parameters):
        super().__init__()
        self.dictionary = {
            parameter['name']: self.scalar(parameter) for parameter in base_parameters.values()
        }

    def scalar(self, parameter):
        if parameter['type'] == "INT" or parameter['type'] == "INT64":
            return ng.p.Scalar(lower=parameter['lower'], upper=parameter['upper']).set_integer_casting()
        elif parameter['type'] == "FLOAT":
            return ng.p.Scalar(lower=parameter['lower'], upper=parameter['upper'])

    def __getitem__(self, key):
        return self.dictionary[key]

    def __iter__(self):
        return iter(self.dictionary)

    def __len__(self):
        return len(self.dictionary)
