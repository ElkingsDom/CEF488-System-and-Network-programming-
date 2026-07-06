CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -O2
SRC_DIR = src

all: server_tcp server_udp client bench

server_tcp: $(SRC_DIR)/kv_server_tcp.c $(SRC_DIR)/kv_store.c
	$(CC) $(CFLAGS) $(SRC_DIR)/kv_server_tcp.c $(SRC_DIR)/kv_store.c -o server_tcp

server_udp: $(SRC_DIR)/kv_server_udp.c $(SRC_DIR)/kv_store.c
	$(CC) $(CFLAGS) $(SRC_DIR)/kv_server_udp.c $(SRC_DIR)/kv_store.c -o server_udp

client: $(SRC_DIR)/kv_client.c
	$(CC) $(CFLAGS) $(SRC_DIR)/kv_client.c -o client

bench: $(SRC_DIR)/kv_bench.c
	$(CC) $(CFLAGS) $(SRC_DIR)/kv_bench.c -o bench

clean:
	rm -f server_tcp server_udp client bench
