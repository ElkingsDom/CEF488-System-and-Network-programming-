#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include "../include/kv_protocol.h"

static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <host> <port> <0=Nagle On, 1=TCP_NODELAY>\n", argv[0]);
        return -1;
    }

    const char *host = argv[1];
    const char *port = argv[2];
    int use_nodelay = atoi(argv[3]);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) return -1;

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1 || connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("Connection failed");
        freeaddrinfo(res);
        return -1;
    }

    if (use_nodelay) {
        int flag = 1;
        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
    }

    struct kv_header hdr;
    hdr.version = htons(PROTOCOL_VERSION);
    hdr.opcode = htons(OP_SET);
    
    char payload[MAX_PAYLOAD];
    memcpy(payload, "benchkey", 9);
    memcpy(payload + 9, "benchvalue", 11);
    uint32_t p_len = 9 + 11;
    hdr.length = htonl(p_len);

    printf("Running 1000 sequential operations... ");
    fflush(stdout);

    double start_time = get_time_ms();

    for (int i = 0; i < 1000; i++) {
        send(sockfd, &hdr, sizeof(struct kv_header), 0);
        send(sockfd, payload, p_len, 0);

        struct kv_header resp_hdr;
        recv(sockfd, &resp_hdr, sizeof(struct kv_header), MSG_WAITALL);
    }

    double end_time = get_time_ms();
    printf("Done.\nTotal Time Elapsed: %.2f ms\n", end_time - start_time);

    close(sockfd);
    freeaddrinfo(res);
    return 0;
}
