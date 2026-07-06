#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/select.h>

#include <arpa/inet.h>

#include "tun.h"
#include "udp.h"
#include "protocol.h"
#include "crypto.h"

#define PORT 5555
#define BUFFER_SIZE 2000

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("Usage: %s <peer-ip>\n", argv[0]);
        return 1;
    }

    char *peer_ip = argv[1];

    char tun_name[20] = "tun0";

    int tun_fd;
    int sockfd;

    unsigned char packet[BUFFER_SIZE];

    uint32_t seq = 1;

    fd_set readfds;

    tun_fd = tun_alloc(tun_name);

    if(tun_fd < 0)
    {
        printf("Failed to create TUN interface\n");
        return 1;
    }

    printf("Created interface: %s\n", tun_name);

    sockfd = create_udp_socket(PORT);

    while(1)
    {
        FD_ZERO(&readfds);

        FD_SET(tun_fd, &readfds);
        FD_SET(sockfd, &readfds);

        int maxfd =
            (tun_fd > sockfd)
            ? tun_fd
            : sockfd;

        if(select(maxfd + 1,
                  &readfds,
                  NULL,
                  NULL,
                  NULL) < 0)
        {
            perror("select");
            break;
        }

        /*
         * TUN -> UDP
         */
        if(FD_ISSET(tun_fd, &readfds))
        {
            int n;

            vpn_header_t *hdr;

            n = read(tun_fd,
                     packet + sizeof(vpn_header_t),
                     BUFFER_SIZE - sizeof(vpn_header_t));

            if(n > 0)
            {
                hdr = (vpn_header_t *)packet;

                hdr->command = CMD_DATA;
                hdr->sequence = htonl(seq++);

                xor_crypt(
                    packet + sizeof(vpn_header_t),
                    n
                );

                send_packet(sockfd,
                            peer_ip,
                            PORT,
                            packet,
                            n + sizeof(vpn_header_t));

                printf("Encrypted and sent seq=%u size=%d\n",
                       seq - 1,
                       n);
            }
        }

        /*
         * UDP -> TUN
         */
        if(FD_ISSET(sockfd, &readfds))
        {
            struct sockaddr_in peer;

            socklen_t peer_len =
                sizeof(peer);

            int n;

            n = recvfrom(sockfd,
                         packet,
                         sizeof(packet),
                         0,
                         (struct sockaddr *)&peer,
                         &peer_len);

            if(n > sizeof(vpn_header_t))
            {
                vpn_header_t *hdr =
                    (vpn_header_t *)packet;

                int payload_size =
                    n - sizeof(vpn_header_t);

                xor_crypt(
                    packet + sizeof(vpn_header_t),
                    payload_size
                );

                write(tun_fd,
                      packet + sizeof(vpn_header_t),
                      payload_size);

                printf("Received and decrypted seq=%u size=%d\n",
                       ntohl(hdr->sequence),
                       payload_size);
            }
        }
    }

    return 0;
}
