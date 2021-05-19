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

import sys

if len(sys.argv) < 2:
    print("Usage: %s output_blob")
    sys.exit(1)

with open(sys.argv[1], "wb") as f:
    f.write(person.SerializeToString())
