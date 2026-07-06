#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <arpa/inet.h>

#include "udp.h"

#define PORT 5555

int main(int argc, char *argv[])
{
    int sockfd;

    char buffer[1024];

    struct sockaddr_in peer;

    socklen_t peer_len = sizeof(peer);

    sockfd = create_udp_socket(PORT);

    printf("Listening on UDP port %d\n", PORT);

    while(1)
    {
        int n = recvfrom(sockfd,
                         buffer,
                         sizeof(buffer)-1,
                         0,
                         (struct sockaddr *)&peer,
                         &peer_len);

        if(n > 0)
        {
            buffer[n] = '\0';

            printf("Received: %s\n", buffer);

            sendto(sockfd,
                   "ACK",
                   3,
                   0,
                   (struct sockaddr *)&peer,
                   peer_len);
        }
    }

    return 0;
}
