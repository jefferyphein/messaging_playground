Previous examples used a `BlockingConnection` to communicate with the
broker. This is easier to code (open connection, send commands, get
data) but has several disadvantages:

- Every interaction requires you to make a request to the broker.
  There is no way for the broker to notify /you/ that data is available.
  The constant polling for data is bad for performance.
- As the name implies the connection runs in the current thread and
  blocks whenever you interact with it. This means that if you don't
  interact with the broker periodically, it will kill your connection
  for inactivity.

A more performant way to interact with Rabbit is using one of several
"asynchronous" connection adapters. In this example we use
`SelectConnection`.

This example will create a consumer that simply echos the bodies of
messages it receives.

First, recall how opening a connection and starting a consumer work
with a blocking connection:

```python
connection = pika.BlockingConnection(params)
channel = connection.channel()
q_ok = channel.queue_declare('', exclusive=True, auto_delete=True)
channel.basic_consume(q_ok.method.queue,
                      on_message_callback=work_function)
# This call blocks. We wait here forever for the consumer to be closed.
```

Notice that each call returns the result we need for the next.
`connection.channel()` blocks, waits for the channel to open and then
returns the channel we need to declare a queue, which returns the
result we need to start the consumer. `SelectConnection` does not do
that. A call to `connection.channel()` is a request to someday open a
channel. Since the channel doesn't exist yet, we can't interact with
it. Instead we have to use a series of "callback" functions to
interact with the broker.

Let's repeat the same example using a `SelectConnection`

First, assume we have a `pika.ConnectionParameters`  that
will let us connect to our broker. Creating a connection is simple,
almost like a `BlockingConnection`:

```python
connection = pika.SelectConnection(params, on_open_callback=open_channel)
```

The difference between this and the `BlockingConnection` is the
`on_open_callback`. All methods on `SelectConnection` are
"asynchronous". The connection will be made someday, but the current
thread doesn't block and wait for it. The `on_open_callback` function
will be called (with the connection as it's only argument) when the
connection is finally opened.

```python
def open_channel(conn):
    channel = conn.channel(on_open_callback=declare_queue)
    return
```

When the connection is opened, we want to open a channel and on that
channel we want to declare a queue. Of course, this is also only a
request to schedule opening the channel so we have to add another
callback to run when the channel opens. Once the queue has been
declared we want to start a consumer on it:

```python
def start_consumer(channel, method):
    print(method)
    print("Starting consumer on %s"%method.method.queue)
    consumer = channel.basic_consume(method.method.queue, on_message_callback=work_function)

def declare_queue(channel):
    queue_cb = functools.partial(start_consumer, channel)
    channel.queue_declare('', exclusive=True, auto_delete=True, callback=queue_cb)
```

The `declare_queue` function creates the queue and, as before,
schedules a callback to start a consumer. The `queue_declare` callback
will receive a single argument, a AMQP `Queue.DeclareOK` method (which
will contain the name of the queue that was declared).

Notice that the `start_consumer` function also needs the channel so
that it can call `basic_consume`. It is common for callbacks to need
more data than their arguments hold. You can use any of several
methods for currying data into the function. Here we use Python's
built-in `functools.partial`.

Now, we've scheduled all of our setup and we need it to actually run.
To do that we have to start the event loop that drives the connection:

```python
connection.ioloop.start()
```

without this nothing ever blocks the current thread and we exit
immediately without doing anything!

Now leave this running and push a message into the queue using one the
previous publishing examples. It works exactly like 
