#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <arpa/inet.h>

#include "udp.h"

int create_udp_socket(int port)
{
    int sockfd;

    struct sockaddr_in addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0)
    {
        perror("socket");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sockfd,
            (struct sockaddr *)&addr,
            sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    return sockfd;
}

int send_packet(int sockfd,
                const char *ip,
                int port,
                void *buffer,
                int len)
{
    struct sockaddr_in peer;

    memset(&peer, 0, sizeof(peer));

    peer.sin_family = AF_INET;
    peer.sin_port = htons(port);

    inet_pton(AF_INET,
              ip,
              &peer.sin_addr);

    int sent = sendto(sockfd,
                  buffer,
                  len,
                  0,
                  (struct sockaddr *)&peer,
                  sizeof(peer));

    if(sent < 0)
    {
        perror("sendto failed");
    }
    else if(sent != len)
    {
        printf("WARNING: short sendto: sent %d of %d bytes\n", sent, len);
    }

    return sent;
}
