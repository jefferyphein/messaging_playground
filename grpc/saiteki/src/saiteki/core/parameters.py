import collections
import json
import saiteki

class Parameter(collections.abc.Mapping):
    NUMERIC_TYPES = [ "INT", "INT64", "FLOAT" ]
    VALID_TYPES = NUMERIC_TYPES

    def __init__(self, name, type, lower, upper, start, *args, **kwargs):
        super().__init__()

        # Ensure input type is valid.
        if type not in self.__class__.VALID_TYPES:
            raise ValueError(f"Unexpected parameter type: {type}")

        # Verify lower <= upper for all numeric types.
        if type in self.__class__.NUMERIC_TYPES:
            if upper < lower:
                raise ValueError(f"Invalid boundary for parameter '{name}': upper bound {upper} must be >= lower bound {lower}")

        # Ensure start point is within bounds.
        if start < lower or start > upper:
            raise ValueError(f"Start value for parameter '{name}' must be in range [{lower},{upper}]; current value: {start}")

        self.dictionary = collections.OrderedDict(
            name=name,
            type=type.upper().strip(),
            lower=lower,
            upper=upper,
            start=start,
        )

    def __getitem__(self, key):
        return self.dictionary[key]

    def __setitem__(self, key, value):
        self.dictionary[key] = value

    def __iter__(self):
        return iter(self.dictionary)

    def __len__(self):
        return len(self.dictionary)

    def __hash__(self):
        return hash(self['name'])

    def __str__(self):
        return ("{{"+", ".join("%s: {%s}" % (key,key) for key in self.dictionary.keys())+"}}").format(**self)

class Constraint:
    VALID_CONSTRAINT_OPS = dict(
        lt  = (lambda x,y: x< y),
        lte = (lambda x,y: x<=y),
        gt  = (lambda x,y: x> y),
        gte = (lambda x,y: x>=y),
        ne  = (lambda x,y: x!=y),
    )

    def __init__(self, parameters, type, op1, op2):
        if op1 not in parameters:
            raise ValueError(f"Unable to apply constraint: parameter '{op1}' does not exist.")
        if op2 not in parameters:
            raise ValueError(f"Unable to apply constraint: parameter '{op2}' does not exist.")
        if op1 == op2:
            raise ValueError(f"Unable to apply constraint: operands must differ, '{op1}'")
        if type not in self.__class__.VALID_CONSTRAINT_OPS.keys():
            raise ValueError(f"Invalid constraint operator '{type}'. Valid types: {list(self.__class__.VALID_CONSTRAINT_OPS.keys())}")

        self.operand1 = op1
        self.operand2 = op2
        self.operator = self.__class__.VALID_CONSTRAINT_OPS[type]
        self.type_ = type

    def __call__(self, dictionary):
        return self.operator(dictionary[self.operand1], dictionary[self.operand2])

    def __str__(self):
        return f"{{ op1: {self.operand1}, type: {self.type_}, op2: {self.operand2} }}"

class Parameters(collections.abc.Mapping):
    def __init__(self, parameters, objective_function, constraints=list(), *args, **kwargs):
        super().__init__()
        self.ordered_parameters = list(Parameter(**param) for param in parameters)
        self.parameters = { param['name']: param for param in self.ordered_parameters }
        self.objective_function = objective_function
        self.constraints = list(Constraint(self.parameters, **constraint) for constraint in constraints)

    def protobuf_request(self, dictionary):
        parameters = list()
        for key,item in self.parameters.items():
            parameter = saiteki.protobuf.Parameter(name=key)
            if self.parameters[key]['type'] == "INT":
                parameter.int32_value = dictionary[key]
            elif self.parameters[key]['type'] == "INT64":
                parameter.int64_value = dictionary[key]
            elif self.parameters[key]['type'] == "FLOAT":
                parameter.float_value = dictionary[key]
            parameters.append(parameter)
        return saiteki.protobuf.CandidateRequest(
            objective_function_json=json.dumps(self.objective_function, sort_keys=True),
            parameters=parameters,
        )

    def update_start_candidate(self, candidate_dict):
        for key in candidate_dict:
            self.parameters[key]['start'] = candidate_dict[key]

    def candidate_dict(self):
        return { name: param['start'] for name,param in self.parameters.items() }

    def __getitem__(self, key):
        return self[key]

    def __iter__(self):
        return iter(__dict__)

    def __len__(self):
        return len(self)

    def __contains__(self, key):
        return key in self.parameters

    def keys(self):
        return self.parameters.keys()

    def items(self):
        return self.parameters.items()

    def values(self):
        return self.parameters.values()

    def _str_objective_function(self):
        return f"objective_function: {self.objective_function}"

    def _str_parameters(self):
        return """parameters:
{parameters}""".format(parameters="\n".join("- "+str(param) for param in self.ordered_parameters))

    def _str_constraints(self):
        if len(self.constraints) > 0:
            return """constraints:
{constraints}""".format(constraints="\n".join("- "+str(constraint) for constraint in self.constraints))
        else:
            return ""

    def __str__(self):
        return f"""---
{self._str_objective_function()}
{self._str_parameters()}
{self._str_constraints()}"""
