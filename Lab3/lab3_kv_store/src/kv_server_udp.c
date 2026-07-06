#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include "../include/kv_protocol.h"
#include "../include/kv_store.h"

#define PORT "8888"

static int make_socket_non_blocking(int sfd) {
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
    int server_fd;
    struct addrinfo hints, *result, *rp;
    
    kv_store_init();

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &result) != 0) {
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server_fd == -1) continue;

        int ipv6only = 0;
        if (setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6only, sizeof(ipv6only)) == -1) {
            close(server_fd);
            continue;
        }

        int reuse = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
            close(server_fd);
            continue;
        }

        if (bind(server_fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(server_fd);
    }

    freeaddrinfo(result);
    if (!rp) return -1;

    if (make_socket_non_blocking(server_fd) == -1) {
        close(server_fd);
        return -1;
    }

    char buffer[sizeof(struct kv_header) + MAX_PAYLOAD];

    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        ssize_t n = recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000); /* Prevent high CPU utilization */
                continue;
            }
            continue;
        }

        if ((size_t)n < sizeof(struct kv_header)) continue;

        struct kv_header *req_header = (struct kv_header *)buffer;
        uint16_t version = ntohs(req_header->version);
        uint16_t opcode = ntohs(req_header->opcode);
        uint32_t length = ntohl(req_header->length);

        if (version != PROTOCOL_VERSION || length > MAX_PAYLOAD || (size_t)n < sizeof(struct kv_header) + length) {
            struct kv_header resp_header = {
                .version = htons(PROTOCOL_VERSION),
                .opcode = htons(STATUS_ERROR),
                .length = 0
            };
            sendto(server_fd, &resp_header, sizeof(resp_header), 0, (struct sockaddr *)&client_addr, client_len);
            continue;
        }

        char *payload = buffer + sizeof(struct kv_header);
        
        char resp_buffer[sizeof(struct kv_header) + MAX_PAYLOAD];
        struct kv_header *resp_header = (struct kv_header *)resp_buffer;
        resp_header->version = htons(PROTOCOL_VERSION);
        resp_header->length = 0;

        if (opcode == OP_SET) {
            char *key = payload;
            size_t key_len = strlen(key);
            if (key_len + 1 >= length) {
                resp_header->opcode = htons(STATUS_ERROR);
            } else {
                char *val = payload + key_len + 1;
                if (kv_store_set(key, val) == 0) {
                    resp_header->opcode = htons(STATUS_SUCCESS);
                } else {
                    resp_header->opcode = htons(STATUS_ERROR);
                }
            }
            sendto(server_fd, resp_buffer, sizeof(struct kv_header), 0, (struct sockaddr *)&client_addr, client_len);
        }
        else if (opcode == OP_GET) {
            char val_out[MAX_VAL_SIZE];
            if (kv_store_get(payload, val_out, MAX_VAL_SIZE) == 0) {
                resp_header->opcode = htons(STATUS_SUCCESS);
                uint32_t val_len = strlen(val_out) + 1;
                resp_header->length = htonl(val_len);
                memcpy(resp_buffer + sizeof(struct kv_header), val_out, val_len);
                sendto(server_fd, resp_buffer, sizeof(struct kv_header) + val_len, 0, (struct sockaddr *)&client_addr, client_len);
            } else {
                resp_header->opcode = htons(STATUS_NOT_FOUND);
                sendto(server_fd, resp_buffer, sizeof(struct kv_header), 0, (struct sockaddr *)&client_addr, client_len);
            }
        }
        else if (opcode == OP_DEL) {
            if (kv_store_del(payload) == 0) {
                resp_header->opcode = htons(STATUS_SUCCESS);
            } else {
                resp_header->opcode = htons(STATUS_NOT_FOUND);
            }
            sendto(server_fd, resp_buffer, sizeof(struct kv_header), 0, (struct sockaddr *)&client_addr, client_len);
        }
        else {
            resp_header->opcode = htons(STATUS_ERROR);
            sendto(server_fd, resp_buffer, sizeof(struct kv_header), 0, (struct sockaddr *)&client_addr, client_len);
        }
    }

    close(server_fd);
    kv_store_destroy();
    return 0;
}
