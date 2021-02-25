import paho.mqtt.publish as publish

publish.single("topic", "Hello, World!", hostname="localhost")
