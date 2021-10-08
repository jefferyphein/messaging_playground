from setuptools import setup, find_packages

__version__ = "0.0.1"

setup(
    name="discovery",
    version=__version__,
    install_requires=[
        "click",
        "grpcio",
        "sqlalchemy",
    ],
    packages=find_packages("."),
    entry_points={
        'console_scripts': [ "discovery=discovery:discovery_cli" ]
    }
)
