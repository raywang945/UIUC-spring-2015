all: client server talker listener

client:
	gcc -o client client.c

server:
	gcc -o server server.c

talker:
	gcc -o talker talker.c

listener:
	gcc -o listener listener.c

.PHONY: clean

clean:
	rm client server talker listener
