import grpc
import protos.hello_pb2

person = protos.hello_pb2.Person(
    name="Bob",
    id=42,
    email="bob@example.com",
    phone=protos.hello_pb2.Person.PhoneNumber(
        number="(123) 456-7890",
        type=protos.hello_pb2.Person.PhoneType.HOME
    )
)

with open("person.blob", "wb") as f:
    f.write(person.SerializeToString())
