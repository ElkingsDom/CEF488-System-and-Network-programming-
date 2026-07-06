#ifndef UDP_H
#define UDP_H

#include <arpa/inet.h>

int create_udp_socket(int port);

int send_packet(int sockfd,
                const char *ip,
                int port,
                void *buffer,
                int len);

#endif
