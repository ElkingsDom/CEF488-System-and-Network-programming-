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
