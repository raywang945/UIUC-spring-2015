CC=gcc

all: http_client http_server https_server

http_client: http_client.c connect_tls.o
	$(CC) -g http_client.c connect_tls.o -o http_client -lpolarssl

http_server: http_server.c
	$(CC) -g http_server.c -o http_server

https_server: https_server.c connect_tls.o
	$(CC) -g https_server.c connect_tls.o -o https_server -lpolarssl

connect_tls.o: connect_tls.c
	$(CC) -c connect_tls.c

.PHONY: clean

clean:
	rm http_client http_server https_server *.o
