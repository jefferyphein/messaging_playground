"Work pool RabbitMQ consumer"

import concurrent.futures
import ssl
import time

import pika

RABBITMQ_HOST = 'localhost'


def work_function(body):
    "Do work. Takes a little while."
    print("Starting work on %s" % body)
    time.sleep(5)
    print("Work on %s finished" % body)


class Consumer:
    "RabbitMQ consumer"
    def __init__(self, params, prefetch, on_message_callback):
        self.connection = pika.SelectConnection(params,
                                                on_open_callback=self.open_channel)
        self.on_message_callback = on_message_callback
        self.prefetch = prefetch
        self.channel = None
        self.queue = None

    def start_consumer(self):
        "Block and start consuming messages"
        self.connection.ioloop.start()  # blocks

    def _setup_queue(self, channel):
        print("Declaring queue")

        def cb(method):
            self.queue = method.method.queue
            print("Starting consumer")
            self.channel.basic_qos(prefetch_count=self.prefetch)
            self.channel.basic_consume(self.queue,
                                       on_message_callback=self.on_message_callback)

        channel.queue_declare('', exclusive=True, auto_delete=True, callback=cb)

    def open_channel(self, connection):
        "Open a channel on the connection, and start a consumer on it."
        print("Opening channel")
        self.channel = self.connection.channel(on_open_callback=self._setup_queue)


def main():
    # logging.basicConfig(level=logging.INFO)
    ctx = ssl.create_default_context(cafile="tls-gen/basic/result/ca_certificate.pem")
    ctx.load_cert_chain("tls-gen/basic/result/client_certificate.pem",
                        keyfile="tls-gen/basic/result/client_key.pem",
                        password="bunnies")
    ssl_opts = pika.SSLOptions(ctx, RABBITMQ_HOST)
    creds = pika.credentials.ExternalCredentials()
    # creds = pika.PlainCredentials(username='guest', password='guest')
    params = pika.ConnectionParameters(RABBITMQ_HOST,
                                       virtual_host='/',
                                       port=5671,
                                       ssl_options=ssl_opts,
                                       credentials=creds)

    pool = concurrent.futures.ThreadPoolExecutor(2)

    def process_message(channel, method, properties, body):
        print("Got message %s" % body)
        task = pool.submit(work_function, body)

        def cb(r):
            channel.basic_ack(method.delivery_tag)

        task.add_done_callback(cb)
        return

    consumer = Consumer(params, prefetch=4, on_message_callback=process_message)
    consumer.start_consumer()  # blocks
    return 0


if __name__ == '__main__':
    main()
