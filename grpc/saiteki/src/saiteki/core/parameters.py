"""Saiteki optimization parameters."""

import collections
import json
import saiteki


class Parameter(collections.abc.Mapping):
    """A single parameter."""

    NUMERIC_TYPES = ["INT", "INT64", "FLOAT"]
    VALID_TYPES = NUMERIC_TYPES

    def __init__(self, name, type, lower, upper, start, *args, **kwargs):
        """Construct a single optimization parameter.

        Arguments:
            name: The parameter name.
            type: The parameter type.
            lower: The lower bound for this parameter.
            upper: The upper bound for this parameter.
            start: The starting value for this parameter.
        """
        super().__init__()

        # Ensure input type is valid.
        if type not in self.__class__.VALID_TYPES:
            raise ValueError(f"Unexpected parameter type: {type}")

        # Verify lower <= upper for all numeric types.
        if type in self.__class__.NUMERIC_TYPES:
            if upper < lower:
                raise ValueError(
                    f"Invalid boundary for parameter '{name}': upper bound {upper} must be >= lower bound {lower}"
                )

        # Ensure start point is within bounds.
        if start < lower or start > upper:
            raise ValueError(
                f"Start value for parameter '{name}' must be in range [{lower},{upper}]; current value: {start}"
            )

        self.dictionary = collections.OrderedDict(
            name=name,
            type=type.upper().strip(),
            lower=lower,
            upper=upper,
            start=start,
        )

    def __getitem__(self, key):
        """Get item from mapping."""
        return self.dictionary[key]

    def __setitem__(self, key, value):
        """Set value of internal mapping."""
        self.dictionary[key] = value

    def __iter__(self):
        """Return interator for mapping."""
        return iter(self.dictionary)

    def __len__(self):
        """Return length of mapping."""
        return len(self.dictionary)

    def __hash__(self):
        """Hash of the parameter."""
        return hash(self['name'])

    def __str__(self):
        """YAML representation of the parameter."""
        return ("{{"+", ".join("%s: {%s}" % (key, key) for key in self.dictionary.keys())+"}}").format(**self)


class Constraint:
    """Constrain optimization."""

    VALID_CONSTRAINT_OPS = dict(
        lt=lambda x, y: x < y,
        lte=lambda x, y: x <= y,
        gt=lambda x, y: x > y,
        gte=lambda x, y: x >= y,
        ne=lambda x, y: x != y,
    )

    def __init__(self, parameters, type, op1, op2):
        """Construct an optimization constraint on two parameters.

        Arguments:
            type: The type of constraint. Valid options are "lt" (less than),
                "lte" (less than or equal to), "gt" (greater than), "gte" (greater
                than or equal to), and "ne" (not equal).
            op1: Name of the left hand side parameter to constrain.
            op2: Name of the right hand side parameter to constrain.

        Example: Constraint(..., "lte", "A", "B") will constrain the optimization
            so that A <= B.
        """
        if op1 not in parameters:
            raise ValueError(f"Unable to apply constraint: parameter '{op1}' does not exist.")
        if op2 not in parameters:
            raise ValueError(f"Unable to apply constraint: parameter '{op2}' does not exist.")
        if op1 == op2:
            raise ValueError(f"Unable to apply constraint: operands must differ, '{op1}'")
        if type not in self.__class__.VALID_CONSTRAINT_OPS.keys():
            raise ValueError(
                f"Invalid constraint operator '{type}'. Valid types: "
                f"{list(self.__class__.VALID_CONSTRAINT_OPS.keys())}"
            )

        self.operand1 = op1
        self.operand2 = op2
        self.operator = self.__class__.VALID_CONSTRAINT_OPS[type]
        self.type_ = type

    def __call__(self, candidate_dict):
        """Evaluate the constraint on the provided dictionary.

        Arguments:
            candidate_dicti: A dictionary containing key-value pairs corresponding
                to the candidate.

        Returns: A boolean indicating whether the constraint is met.
        """
        return self.operator(candidate_dict[self.operand1], candidate_dict[self.operand2])

    def __str__(self):
        """YAML representation of the constraint."""
        return f"{{ op1: {self.operand1}, type: {self.type_}, op2: {self.operand2} }}"


class Parameters(collections.abc.Mapping):
    """Container for many `saiteki.core.Parameter` objects."""

    def __init__(self, parameters, objective_function, constraints=list(), *args, **kwargs):
        """Construct a parameter list.

        Arguments:
            parameters: A list of dictionaries used to construct invididual
                `saiteki.core.Parameter` objects.
            objective_function: A dictionary containing the objective function
                configuration.
            constraints: A list of constraints.
        """
        super().__init__()
        self.ordered_parameters = list(Parameter(**param) for param in parameters)
        self.parameters = {param['name']: param for param in self.ordered_parameters}
        self.objective_function = objective_function
        self.constraints = list(Constraint(self.parameters, **constraint) for constraint in constraints)

    def protobuf_request(self, candidate_dict):
        """Convert a candidate dictionary into a Protobuf request.

        Arguments:
            candidate_dict: A dictionary representing the candidate.

        Returns: A `saiteki.protobuf.CandidateRequest` object.
        """
        parameters = list()
        for key, item in self.parameters.items():
            parameter = saiteki.protobuf.Parameter(name=key)
            if self.parameters[key]['type'] == "INT":
                parameter.int32_value = candidate_dict[key]
            elif self.parameters[key]['type'] == "INT64":
                parameter.int64_value = candidate_dict[key]
            elif self.parameters[key]['type'] == "FLOAT":
                parameter.float_value = candidate_dict[key]
            parameters.append(parameter)
        return saiteki.protobuf.CandidateRequest(
            objective_function_json=json.dumps(self.objective_function, sort_keys=True),
            parameters=parameters,
        )

    def update_start_candidate(self, candidate_dict):
        """Update the start candidate.

        Updates the start value of each `saiteki.core.Parameter` object held
        by this object.

        Arguments:
            candidate_dict: A dictionary representing the candidate.
        """
        for key in candidate_dict:
            self.parameters[key]['start'] = candidate_dict[key]

    def candidate_dict(self):
        """Convert the start values of reach parameter into a dictionary."""
        return {name: param['start'] for name, param in self.parameters.items()}

    def __getitem__(self, key):
        """Get the `saiteki.core.Parameter` object associated to the key."""
        return self[key]

    def __iter__(self):
        """Get the parameter iterator."""
        return iter(self.__dict__)

    def __len__(self):
        """Get the number of parameters."""
        return len(self)

    def __contains__(self, key):
        """Determine whether there is a parameter with the input value."""
        return key in self.parameters

    def keys(self):
        """Get the list of parameter names."""
        return self.parameters.keys()

    def items(self):
        """Get key-value pairs for all parameters."""
        return self.parameters.items()

    def values(self):
        """Get the values for all parameters."""
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
        """Return a YAML string for the current state."""
        return f"""---
{self._str_objective_function()}
{self._str_parameters()}
{self._str_constraints()}"""
