#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/select.h>

#include <arpa/inet.h>

#include "tun.h"
#include "udp.h"
#include "protocol.h"
#include "crypto.h"

#define PORT 5555
#define BUFFER_SIZE 2000

#define KEEPALIVE_INTERVAL 5
#define PEER_TIMEOUT 15

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

    time_t last_keepalive_sent;
    time_t last_packet_received;

    int peer_alive = 1;

    tun_fd = tun_alloc(tun_name);

    if(tun_fd < 0)
    {
        printf("Failed to create TUN interface\n");
        return 1;
    }

    printf("Created interface: %s\n", tun_name);

    sockfd = create_udp_socket(PORT);

    last_keepalive_sent = time(NULL);
    last_packet_received = time(NULL);

    while(1)
    {
        struct timeval tv;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&readfds);

        FD_SET(tun_fd, &readfds);
        FD_SET(sockfd, &readfds);

        int maxfd =
            (tun_fd > sockfd)
            ? tun_fd
            : sockfd;

        select(maxfd + 1,
               &readfds,
               NULL,
               NULL,
               &tv);

        time_t now = time(NULL);

        /*
         * Send keepalive every 5 seconds
         */
        if(now - last_keepalive_sent >= KEEPALIVE_INTERVAL)
        {
            vpn_header_t *hdr =
                (vpn_header_t *)packet;

            hdr->command = CMD_KEEPALIVE;
            hdr->sequence = htonl(seq++);

            send_packet(sockfd,
                        peer_ip,
                        PORT,
                        packet,
                        sizeof(vpn_header_t));

            printf("KEEPALIVE sent\n");

            last_keepalive_sent = now;
        }

        /*
         * Detect timeout
         */
        if(peer_alive &&
           (now - last_packet_received) > PEER_TIMEOUT)
        {
            printf("\n*** PEER TIMEOUT ***\n\n");

            peer_alive = 0;
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

            if(n >= sizeof(vpn_header_t))
            {
                vpn_header_t *hdr =
                    (vpn_header_t *)packet;

                last_packet_received = now;

                if(!peer_alive)
                {
                    printf("\n*** PEER RECONNECTED ***\n\n");

                    peer_alive = 1;
                }

                if(hdr->command == CMD_KEEPALIVE)
                {
                    printf("KEEPALIVE received\n");
                    continue;
                }

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
