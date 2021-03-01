all: server client

%: %.c
	gcc -o $@ $< -lzmq -pthread

clean:
	rm -f server client
