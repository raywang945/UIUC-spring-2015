CC=gcc

all: client server talker listener

client: client.c
	$(CC) -o client client.c

server: server.c
	$(CC) -o server server.c

talker: talker.c
	$(CC) -o talker talker.c

listener: listener.c
	$(CC) -o listener listener.c

.PHONY: clean

clean:
	rm client server talker listener
