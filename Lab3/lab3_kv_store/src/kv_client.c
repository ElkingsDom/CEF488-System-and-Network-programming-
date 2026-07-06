#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include "../include/kv_protocol.h"

#define DEFAULT_TIMEOUT_MS 200
#define MAX_RETRIES 3

static int send_tcp(const char *host, const char *port, struct kv_header *hdr, const char *payload, uint32_t payload_len) {
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) return -1;

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        freeaddrinfo(res);
        return -1;
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    send(sockfd, hdr, sizeof(struct kv_header), 0);
    if (payload_len > 0) {
        send(sockfd, payload, payload_len, 0);
    }

    struct kv_header resp_hdr;
    if (recv(sockfd, &resp_hdr, sizeof(struct kv_header), MSG_WAITALL) <= 0) {
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    uint16_t opcode = ntohs(resp_hdr.opcode);
    uint32_t length = ntohl(resp_hdr.length);

    if (opcode == STATUS_SUCCESS) {
        printf("OK\n");
        if (length > 0 && length < MAX_PAYLOAD) {
            char resp_payload[MAX_PAYLOAD];
            recv(sockfd, resp_payload, length, MSG_WAITALL);
            printf("%s\n", resp_payload);
        }
    } else if (opcode == STATUS_NOT_FOUND) {
        printf("NOT FOUND\n");
    } else {
        printf("ERROR\n");
    }

    close(sockfd);
    freeaddrinfo(res);
    return 0;
}

static int send_udp(const char *host, const char *port, struct kv_header *hdr, const char *payload, uint32_t payload_len) {
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) return -1;

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        freeaddrinfo(res);
        return -1;
    }

    char buf[sizeof(struct kv_header) + MAX_PAYLOAD];
    memcpy(buf, hdr, sizeof(struct kv_header));
    if (payload_len > 0) {
        memcpy(buf + sizeof(struct kv_header), payload, payload_len);
    }
    size_t total_len = sizeof(struct kv_header) + payload_len;

    int timeout_ms = DEFAULT_TIMEOUT_MS;
    int retry = 0;
    int success = 0;
    struct kv_header resp_hdr;
    char resp_payload[MAX_PAYLOAD];
    ssize_t n = 0;

    while (retry <= MAX_RETRIES) {
        sendto(sockfd, buf, total_len, 0, res->ai_addr, res->ai_addrlen);

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        n = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL);
        if (n >= 0) {
            if ((size_t)n >= sizeof(struct kv_header)) {
                memcpy(&resp_hdr, buf, sizeof(struct kv_header));
                uint32_t r_len = ntohl(resp_hdr.length);
                if (r_len > 0 && (size_t)n >= sizeof(struct kv_header) + r_len) {
                    memcpy(resp_payload, buf + sizeof(struct kv_header), r_len);
                }
                success = 1;
                break;
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                retry++;
                timeout_ms *= 2; /* Exponential backoff */
                continue;
            }
            break;
        }
    }

    if (!success) {
        printf("TIMEOUT ERROR\n");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    uint16_t opcode = ntohs(resp_hdr.opcode);
    uint32_t length = ntohl(resp_hdr.length);

    if (opcode == STATUS_SUCCESS) {
        printf("OK\n");
        if (length > 0 && length < MAX_PAYLOAD) {
            printf("%s\n", resp_payload);
        }
    } else if (opcode == STATUS_NOT_FOUND) {
        printf("NOT FOUND\n");
    } else {
        printf("ERROR\n");
    }

    close(sockfd);
    freeaddrinfo(res);
    return 0;
}

int main(int argc, char *argv[]) {
    int use_udp = 0;
    int arg_idx = 1;

    if (argc < 5) {
        fprintf(stderr, "Usage: %s [-u] <host> <port> <cmd> <key> [val]\n", argv[0]);
        return -1;
    }

    if (strcmp(argv[1], "-u") == 0) {
        use_udp = 1;
        arg_idx++;
    }

    if ((argc - arg_idx) < 4 && strcmp(argv[arg_idx + 2], "SET") == 0) {
        return -1;
    }

    const char *host = argv[arg_idx];
    const char *port = argv[arg_idx + 1];
    const char *cmd = argv[arg_idx + 2];
    const char *key = argv[arg_idx + 3];

    struct kv_header hdr;
    hdr.version = htons(PROTOCOL_VERSION);
    hdr.length = 0;

    char payload[MAX_PAYLOAD];
    memset(payload, 0, sizeof(payload));
    uint32_t payload_len = 0;

    if (strcmp(cmd, "SET") == 0) {
        hdr.opcode = htons(OP_SET);
        const char *val = argv[arg_idx + 4];
        size_t k_len = strlen(key);
        size_t v_len = strlen(val);
        
        memcpy(payload, key, k_len + 1);
        memcpy(payload + k_len + 1, val, v_len + 1);
        payload_len = k_len + 1 + v_len + 1;
        hdr.length = htonl(payload_len);
    } 
    else if (strcmp(cmd, "GET") == 0) {
        hdr.opcode = htons(OP_GET);
        payload_len = strlen(key) + 1;
        memcpy(payload, key, payload_len);
        hdr.length = htonl(payload_len);
    } 
    else if (strcmp(cmd, "DEL") == 0) {
        hdr.opcode = htons(OP_DEL);
        payload_len = strlen(key) + 1;
        memcpy(payload, key, payload_len);
        hdr.length = htonl(payload_len);
    } 
    else {
        return -1;
    }

    if (use_udp) {
        send_udp(host, port, &hdr, payload, payload_len);
    } else {
        send_tcp(host, port, &hdr, payload, payload_len);
    }

    return 0;
}
