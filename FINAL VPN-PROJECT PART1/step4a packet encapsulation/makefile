CC=gcc

CFLAGS=-Wall

SRC=src/main.c src/tun.c src/udp.c  src/protocol.c

vpn:
	$(CC) $(CFLAGS) $(SRC) -o vpn

clean:
	rm -f vpn
