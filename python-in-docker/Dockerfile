# Use a slim Python Docker image to keep things small. The TAG argument may be passed as a build
# argument if a different image is desired.
ARG TAG=3.12-slim-bullseye
FROM python:$TAG

# Default to user/group 1000, but allow them to be provided as build arguments. The launcher script
# will provide these based on the user/group of whomever runs the launcher.
ARG UID=1000
ARG GID=1000

# Create a default user group, both with names "user" to simplify things. Inside of the container,
# the "user" will map to whomever launched the application.
RUN groupadd --gid $GID user && \
    useradd --gid $GID --uid $UID --create-home user

# Before we switch from the root user to the runtime user, let's create a dedicated directory for
# creating virtual environments in the /venv directory to keep things short.
RUN mkdir /venv && chown user:user /venv

# Switch to the user and set the working directory where source code should be mounted.
USER user
WORKDIR /var/src

# Create virtual environment and update pip.
RUN python -m venv /venv && \
    . /venv/bin/activate && \
    pip install -U pip

# Run /bin/bash so that the container doesn't exit immediately. This allow us to stop the container,
# restart it, and attach to it without losing state.
CMD ["/bin/bash"]
