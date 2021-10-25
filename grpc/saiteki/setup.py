from setuptools import setup, find_packages

__version__ = "2.0.0"

setup(
    name="saiteki",
    version=__version__,
    install_requires=[
        "click",
        "grpcio",
        "nevergrad",
        "protobuf",
        "pyyaml",
        "sqlalchemy",
    ],
    packages=find_packages("src"),
    package_dir={'': 'src'},
    entry_points={
        'console_scripts': ["saiteki=saiteki:saiteki_cli"]
    },
    include_package_data=True
)
