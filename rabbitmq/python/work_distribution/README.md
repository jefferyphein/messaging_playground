A common situation is that you have work requests coming in from
Somewhere and wish to apply some function to them.  The obvious
solution is something like:

```python
def do_work(body):
   # do stuff here
   
def on_message(channel, method, properties, body):
    do_work(body)
    channel.ack(method.delivery_tag)
    return
    
channel.basic_consume("some queue", on_message)
```
Create a consumer that pulls down messages as they arrive, applies
your work function to the body and then acks the message.  If you want
to process more messages, simply open more consumers.

The observant user will note an issue with this approach. Suppose your
work function takes at least as long as the time to download a new
message.  You might expect, with all the asynchronous callback stuff
flying around that you can hide the latency of download a message
under the work function:

```
|---download---|---download---|---download---|
               |---work---|   |---work---|
```

But instead you find:

```
|---download---|---work---|---download---|---work---|---download---|
```

which clearly takes much longer.

The reason this happens is because the `on_message_callback` happens
in the event loop that drives the connection, blocking it until
`do_work` is complete.  Even worse, if `do_work` is very long running
(on the order of minutes) you will find that the RabbitMQ broker will
simply terminate your connection because you've failed to send
heartbeat messages!

The solution is simple: don't do work inside the
`on_message_callback`. Instead dispatch the work to somewhere else,
for example, using threads.

```python
pool = concurrent.futures.ThreadPoolExecutor(num_threads)
# Use a ProcessPoolExecutor if you're concerned about the GIL

def on_message(channel, method, properties, body):
    task = pool.submit(do_work, body)
    task.add_done_callback(lambda r: channel.basic_ack(method.delivery_tag))
    return
    
channel.basic_qos(prefetch_count=num_threads+1)
consumer.basic_consume("some_queue", on_message)
```

Now your `on_message_callback` is essentially instant and you can pull
down messages as fast your network allows. Since we otherwise lose
track of the message once we push it into a thread, we have to use a
callback to schedule the ACK (and to publish any follow-on messages).
If you're wondering why we don't simply pass the channel into
`do_work` and call `basic_ack` there: **PIKA CHANNELS ARE NOT
THREADSAFE**! Attaching a callback to the task allows us to run the
function later in the main thread (the one that created the
`ThreadPoolExecutor`) after `do_work` completes.
