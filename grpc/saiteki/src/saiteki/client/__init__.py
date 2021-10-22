import click
import grpc
import saiteki.protobuf

from .. import saiteki_cli

@saiteki_cli.command('client')
@click.option("--key", type=click.Path(exists=True), envvar="KEY", help="PEM private key")
@click.option("--cert", type=click.Path(exists=True), envvar="CERT", help="PEM certificate chain")
@click.option("--cacert", type=click.Path(exists=True), envvar="CACERT", help="Root certificate")
@click.pass_context
def client_cli(ctx, key, cert, cacert, *args, **kwargs):
    """Starts an optimization client."""

    client_key = open(key, "rb").read() if key else None
    client_cert = open(cert, "rb").read() if cert else None
    client_cacert = open(cacert, "rb").read() if cacert else None

    credentials = grpc.ssl_channel_credentials(
        root_certificates=client_cacert,
        private_key=client_key,
        certificate_chain=client_cert,
    )
    channel = grpc.secure_channel(
        "localhost:12345", credentials
    )
    stub = saiteki.protobuf.SaitekiStub(channel)

    request = saiteki.protobuf.ShutdownRequest()
    response = stub.Shutdown(request)
    print(response)
