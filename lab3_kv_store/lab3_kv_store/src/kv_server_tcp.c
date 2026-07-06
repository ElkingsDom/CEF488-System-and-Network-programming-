#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include "../include/kv_protocol.h"
#include "../include/kv_store.h"

#define MAX_EVENTS 64
#define PORT "8888"

static int make_socket_non_blocking(int sfd) {
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
}

static void handle_client_request(int client_fd) {
    struct kv_header header;
    ssize_t count = recv(client_fd, &header, sizeof(struct kv_header), 0);
    
    if (count <= 0) {
        if (count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            close(client_fd);
        }
        return;
    }

    uint16_t version = ntohs(header.version);
    uint16_t opcode = ntohs(header.opcode);
    uint32_t length = ntohl(header.length);

    struct kv_header resp_header;
    resp_header.version = htons(PROTOCOL_VERSION);
    resp_header.length = 0;

    if (version != PROTOCOL_VERSION || length > MAX_PAYLOAD) {
        resp_header.opcode = htons(STATUS_ERROR);
        send(client_fd, &resp_header, sizeof(resp_header), 0);
        return;
    }

    char payload[MAX_PAYLOAD];
    memset(payload, 0, sizeof(payload));
    
    if (length > 0) {
        size_t total_received = 0;
        while (total_received < length) {
            ssize_t n = recv(client_fd, payload + total_received, length - total_received, 0);
            if (n <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                resp_header.opcode = htons(STATUS_ERROR);
                send(client_fd, &resp_header, sizeof(resp_header), 0);
                return;
            }
            total_received += n;
        }
    }

    if (opcode == OP_SET) {
        char *key = payload;
        size_t key_len = strlen(key);
        if (key_len + 1 >= length) {
            resp_header.opcode = htons(STATUS_ERROR);
        } else {
            char *val = payload + key_len + 1;
            if (kv_store_set(key, val) == 0) {
                resp_header.opcode = htons(STATUS_SUCCESS);
            } else {
                resp_header.opcode = htons(STATUS_ERROR);
            }
        }
        send(client_fd, &resp_header, sizeof(resp_header), 0);
    } 
    else if (opcode == OP_GET) {
        char val_out[MAX_VAL_SIZE];
        if (kv_store_get(payload, val_out, MAX_VAL_SIZE) == 0) {
            resp_header.opcode = htons(STATUS_SUCCESS);
            uint32_t val_len = strlen(val_out) + 1;
            resp_header.length = htonl(val_len);
            send(client_fd, &resp_header, sizeof(resp_header), 0);
            send(client_fd, val_out, val_len, 0);
        } else {
            resp_header.opcode = htons(STATUS_NOT_FOUND);
            send(client_fd, &resp_header, sizeof(resp_header), 0);
        }
    } 
    else if (opcode == OP_DEL) {
        if (kv_store_del(payload) == 0) {
            resp_header.opcode = htons(STATUS_SUCCESS);
        } else {
            resp_header.opcode = htons(STATUS_NOT_FOUND);
        }
        send(client_fd, &resp_header, sizeof(resp_header), 0);
    } 
    else {
        resp_header.opcode = htons(STATUS_ERROR);
        send(client_fd, &resp_header, sizeof(resp_header), 0);
    }
}

int main(void) {
    int listen_fd, epoll_fd;
    struct addrinfo hints, *result, *rp;
    struct epoll_event event, events[MAX_EVENTS];

    kv_store_init();

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET6; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &result) != 0) {
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_fd == -1) continue;

        int ipv6only = 0;
        if (setsockopt(listen_fd, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6only, sizeof(ipv6only)) == -1) {
            close(listen_fd);
            continue;
        }

        int reuse = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
            close(listen_fd);
            continue;
        }

        if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(listen_fd);
    }

    freeaddrinfo(result);
    if (!rp) return -1;

    if (make_socket_non_blocking(listen_fd) == -1 || listen(listen_fd, SOMAXCONN) == -1) {
        close(listen_fd);
        return -1;
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        close(listen_fd);
        return -1;
    }

    event.data.fd = listen_fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) == -1) {
        close(listen_fd);
        close(epoll_fd);
        return -1;
    }

    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                close(events[i].data.fd);
                continue;
            }

            if (listen_fd == events[i].data.fd) {
                while (1) {
                    struct sockaddr in_addr;
                    socklen_t in_len = sizeof(in_addr);
                    int infd = accept(listen_fd, &in_addr, &in_len);
                    if (infd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) break;
                        break;
                    }
                    if (make_socket_non_blocking(infd) == -1) {
                        close(infd);
                        continue;
                    }
                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &event);
                }
            } else {
                handle_client_request(events[i].data.fd);
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    kv_store_destroy();
    return 0;
}
