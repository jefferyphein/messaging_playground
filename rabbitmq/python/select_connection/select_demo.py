import functools
import logging
import ssl

import pika

RABBITMQ_HOST='localhost'

def work_function(channel, method, properties, body):
    print("Got message %s"%body)

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




    def start_consumer(channel, method):
        print(method)
        print("Starting consumer on %s"%method.method.queue)
        consumer = channel.basic_consume(method.method.queue, on_message_callback=work_function)

    def declare_queue(channel):
        queue_cb = functools.partial(start_consumer, channel)
        channel.queue_declare('', exclusive=True, auto_delete=True, callback=queue_cb)

    def open_channel(conn):
        # Create a channel, we don't know if the channel is open yet,
        # so we have to schedule a callback before we can declare
        # queues and such
        channel = conn.channel(on_open_callback=declare_queue)
        return



    connection = pika.SelectConnection(params, on_open_callback=open_channel)
    connection.ioloop.start()
