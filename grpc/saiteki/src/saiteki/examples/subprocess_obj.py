"""Subprocess objective function example."""

import subprocess


class SubprocessObjective:
    """Subprocess callable object example."""

    def __init__(self, cmd):
        """Construct the callable objective function object.

        Arguments:
            cmd: The command to run as a subprocess.
        """
        self.cmd = cmd

    # The timeout parameter is always passed by the caller and is set based on
    # the deadline set by the client that originally requested the objective
    # function evaluation. If the client did not set a deadline, timeout will
    # be None. This value can be used to ensure an objective function with a
    # deadline does not run amok and run beyond its expected lifespan. This is
    # not a guarantee that the command called by subprocess will actually
    # terminate, however, it is merely a suggestion. This is not a 100%
    # reliable mechanism to control job flow, as subprocesses can simply ignore
    # incoming signals. Therefore, it is up to the user to ensure that the
    # applications called by subprocess respect termination signal.
    def __call__(self, candidate, timeout=None, *args, **kwargs):
        """Objective function.

        Arguments:
            candidate: The candidate dictionary.
            timeout: The timeout for this call, can be passed as a keyword to
                a call to any subprocess function. May be None.
        """
        output = subprocess.check_output(self.cmd, timeout=timeout)
        return float(output)
