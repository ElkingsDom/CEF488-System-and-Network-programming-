#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

#include "protocol.h"

#define PORT 5555

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        printf("Usage: %s <IP> <message>\n", argv[0]);
        return 1;
    }

    int sockfd;

    struct sockaddr_in peer;

    char packet[2048];

    vpn_header_t *hdr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&peer, 0, sizeof(peer));

    peer.sin_family = AF_INET;
    peer.sin_port = htons(PORT);

    inet_pton(AF_INET, argv[1], &peer.sin_addr);

    hdr = (vpn_header_t *)packet;

    hdr->command = CMD_DATA;
    hdr->sequence = htonl(1);

    strcpy(packet + sizeof(vpn_header_t),
           argv[2]);

    int total =
        sizeof(vpn_header_t) +
        strlen(argv[2]) + 1;

    sendto(sockfd,
           packet,
           total,
           0,
           (struct sockaddr *)&peer,
           sizeof(peer));

    return 0;
}
