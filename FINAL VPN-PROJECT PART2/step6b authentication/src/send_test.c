#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

#include "protocol.h"

#define PORT 5555

/*
 * send_test - simulates a client WITHOUT the correct preshared key.
 *
 * Usage:
 *   ./send_test <IP> <message>              -> sends with an INVALID auth token
 *   ./send_test <IP> <message> <auth-token>  -> sends with a custom token
 *                                                (pass the real AUTH_TOKEN to
 *                                                 prove legitimate packets still
 *                                                 get accepted)
 */
int main(int argc, char *argv[])
{
    if(argc != 3 && argc != 4)
    {
        printf("Usage: %s <IP> <message> [auth-token]\n", argv[0]);
        printf("  (no auth-token given -> sends an INVALID key to test rejection)\n");
        return 1;
    }

    int sockfd;

    struct sockaddr_in peer;

    char packet[2048];

    vpn_header_t *hdr;

    const char *token =
        (argc == 4) ? argv[3] : "INVALID_KEY_0000";

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&peer, 0, sizeof(peer));

    peer.sin_family = AF_INET;
    peer.sin_port = htons(PORT);

    inet_pton(AF_INET, argv[1], &peer.sin_addr);

    memset(packet, 0, sizeof(packet));

    hdr = (vpn_header_t *)packet;

    hdr->command = CMD_DATA;
    hdr->sequence = htonl(1);

    /* deliberately copy whatever token was chosen into the auth field,
     * truncated to fit -- by default this will NOT match AUTH_TOKEN */
    strncpy(hdr->auth, token, sizeof(hdr->auth) - 1);

    strcpy(packet + sizeof(vpn_header_t),
           argv[2]);

    int total =
        sizeof(vpn_header_t) +
        strlen(argv[2]) + 1;

    printf("Sending packet with auth token \"%s\" to %s:%d\n",
           token, argv[1], PORT);

    sendto(sockfd,
           packet,
           total,
           0,
           (struct sockaddr *)&peer,
           sizeof(peer));

    return 0;
}
