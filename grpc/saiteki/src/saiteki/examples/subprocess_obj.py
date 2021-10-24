import subprocess


class SubprocessObjective:
    def __init__(self, cmd):
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
        output = subprocess.check_output(self.cmd, timeout=timeout)
        return float(output)
