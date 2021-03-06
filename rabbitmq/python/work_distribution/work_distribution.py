import concurrent.futures
import queue
import functools
import logging
import ssl
import time

import pika

RABBITMQ_HOST='localhost'

def work_function(body):
    print("Starting work on %s"%body)
    time.sleep(5)
    print("Work on %s finished"%body)

class Consumer:
    def __init__(self, params, prefetch, on_message_callback):
        self.connection = pika.SelectConnection(params, on_open_callback=self.open_channel,
                                                on_open_error_callback=self.open_error)
        self.on_message_callback = on_message_callback
        self.prefetch = prefetch
        self.channel = None
        self.queue = None

    def open_error(self, connection, exc):
        print("connection failed to open with exception %s"%exc)

    def start_consumer(self):
        self.connection.ioloop.start() #blocks

    def setup_queue(self, channel):
        print("Declaring queue")

        def cb(method):
            self.queue = method.method.queue
            print("Starting consumer")
            self.channel.basic_qos(prefetch_count=self.prefetch)
            self.channel.basic_consume(self.queue, on_message_callback=self.on_message_callback)

        channel.queue_declare('', exclusive=True, auto_delete=True, callback=cb)

    def open_channel(self, connection):
        print("Opening channel")
        self.channel = self.connection.channel(on_open_callback=self.setup_queue)


if __name__ == '__main__':
    # logging.basicConfig(level=logging.INFO)
    ctx = ssl.create_default_context(cafile="tls-gen/basic/result/ca_certificate.pem")
    ctx.load_cert_chain("tls-gen/basic/result/client_certificate.pem",
                        keyfile="tls-gen/basic/result/client_key.pem",
                        password="bunnies")
    ssl_opts = pika.SSLOptions(ctx, RABBITMQ_HOST)
    creds =pika.credentials.ExternalCredentials()
    # creds = pika.PlainCredentials(username='guest', password='guest')
    params = pika.ConnectionParameters(RABBITMQ_HOST,
                                       virtual_host='/',
                                       port=5671,
                                       ssl_options=ssl_opts,
                                       credentials=creds)

    pool = concurrent.futures.ThreadPoolExecutor(2)

    def process_message(channel, method, properties, body):
        print("Got message %s"%body)
        task = pool.submit(work_function, body)

        def cb(r):
            channel.basic_ack(method.delivery_tag)

        task.add_done_callback(cb)
        return

    consumer = Consumer(params, prefetch=4, on_message_callback=process_message)
    consumer.start_consumer() # blocks
