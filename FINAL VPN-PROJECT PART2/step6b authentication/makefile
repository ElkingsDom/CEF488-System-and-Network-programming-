CC=gcc
CFLAGS=-Wall
SRC=src/main.c \
    src/tun.c \
    src/udp.c \
    src/protocol.c \
    src/crypto.c
 
TEST_SRC=src/send_test.c \
         src/udp.c \
         src/protocol.c
 
vpn:
	$(CC) $(CFLAGS) $(SRC) -o vpn
 
send_test:
	$(CC) $(CFLAGS) $(TEST_SRC) -o send_test
 
all: vpn send_test
 
clean:
	rm -f vpn send_test
