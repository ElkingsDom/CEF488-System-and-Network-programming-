#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

#include "udp.h"
#include "protocol.h"

#define PORT 5555

int main()
{
    int sockfd;

    char buffer[2048];

    struct sockaddr_in peer;

    socklen_t peer_len = sizeof(peer);

    sockfd = create_udp_socket(PORT);

    printf("Listening on UDP port %d\n", PORT);

    while(1)
    {
        int n = recvfrom(sockfd,
                         buffer,
                         sizeof(buffer),
                         0,
                         (struct sockaddr *)&peer,
                         &peer_len);

        if(n > sizeof(vpn_header_t))
        {
            vpn_header_t *hdr;

            hdr = (vpn_header_t *)buffer;

            printf("\n");
            printf("Command : %u\n", hdr->command);
            printf("Sequence: %u\n", ntohl(hdr->sequence));

            printf("Payload : %s\n",
                   buffer + sizeof(vpn_header_t));
        }
    }

    return 0;
}
