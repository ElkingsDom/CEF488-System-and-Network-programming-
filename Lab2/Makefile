CC = gcc
CFLAGS = -Wall -Wextra

all: server client

server: log_server.c
	$(CC) $(CFLAGS) -o server log_server.c

client: log_client.c
	$(CC) $(CFLAGS) -o client log_client.c

clean:
	rm -f server client

server: log_server.c
	gcc -Wall -Wextra -o server log_server.c -lpthread
