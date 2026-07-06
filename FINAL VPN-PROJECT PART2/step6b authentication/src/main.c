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

static void hexdump(const char *label, unsigned char *buf, int len)
{
    int n = (len < 32) ? len : 32;
    int i;

    printf("%s (%d bytes): ", label, len);

    for(i = 0; i < n; i++)
    {
        printf("%02x ", buf[i]);
    }

    printf("\n");
}

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
         * KEEPALIVE
         */
        if(now - last_keepalive_sent >= KEEPALIVE_INTERVAL)
        {
            vpn_header_t *hdr =
                (vpn_header_t *)packet;

            memset(packet, 0, sizeof(packet));

            hdr->command = CMD_KEEPALIVE;
            hdr->sequence = htonl(seq++);

            strcpy(hdr->auth,
                   AUTH_TOKEN);

            send_packet(sockfd,
                        peer_ip,
                        PORT,
                        packet,
                        sizeof(vpn_header_t));

            printf("KEEPALIVE sent\n");

            last_keepalive_sent = now;
        }

        /*
         * TIMEOUT
         */
        if(peer_alive &&
           (now - last_packet_received) >
           PEER_TIMEOUT)
        {
            printf("\n*** PEER TIMEOUT ***\n\n");

            peer_alive = 0;
        }

        /*
         * TUN -> UDP
         */
        if(FD_ISSET(tun_fd,
                    &readfds))
        {
            int n;

            vpn_header_t *hdr;

            n = read(tun_fd,
                     packet + sizeof(vpn_header_t),
                     BUFFER_SIZE -
                     sizeof(vpn_header_t));

            if(n < 0)
            {
                perror("read from tun_fd failed");
            }

            if(n > 0)
            {
                hdr = (vpn_header_t *)packet;

                hdr->command = CMD_DATA;

                hdr->sequence =
                    htonl(seq++);

                strcpy(hdr->auth,
                       AUTH_TOKEN);

                hexdump("RAW (pre-encrypt)",
                        packet + sizeof(vpn_header_t),
                        n);

                xor_crypt(
                    packet +
                    sizeof(vpn_header_t),
                    n);

                int sent = send_packet(sockfd,
                            peer_ip,
                            PORT,
                            packet,
                            n +
                            sizeof(vpn_header_t));

                printf(
                    "Encrypted and sent seq=%u size=%d sent_bytes=%d\n",
                    seq - 1,
                    n,
                    sent);
            }
        }

        /*
         * UDP -> TUN
         */
        if(FD_ISSET(sockfd,
                    &readfds))
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

            if(n < 0)
            {
                perror("recvfrom failed");
            }

            if(n >= sizeof(vpn_header_t))
            {
                vpn_header_t *hdr =
                    (vpn_header_t *)packet;

                if(memcmp(hdr->auth,
                          AUTH_TOKEN,
                          sizeof(AUTH_TOKEN)) != 0)
                {
                    printf(
                      "Unauthorized packet dropped (source %s:%d, no valid preshared key)\n",
                      inet_ntoa(peer.sin_addr),
                      ntohs(peer.sin_port));

                    continue;
                }

                last_packet_received =
                    time(NULL);

                if(!peer_alive)
                {
                    printf(
                      "\n*** PEER RECONNECTED ***\n\n");

                    peer_alive = 1;
                }

                if(hdr->command ==
                   CMD_KEEPALIVE)
                {
                    printf(
                      "KEEPALIVE received\n");

                    continue;
                }

                int payload_size =
                    n -
                    sizeof(vpn_header_t);

                xor_crypt(
                    packet +
                    sizeof(vpn_header_t),
                    payload_size);

                hexdump("DECRYPTED (pre-write to tun)",
                        packet + sizeof(vpn_header_t),
                        payload_size);

                int written = write(tun_fd,
                      packet +
                      sizeof(vpn_header_t),
                      payload_size);

                if(written < 0)
                {
                    perror("write to tun_fd failed");
                }
                else if(written != payload_size)
                {
                    printf("WARNING: short write to tun: wrote %d of %d bytes\n",
                           written, payload_size);
                }

                printf(
                    "Received and decrypted seq=%u size=%d written=%d\n",
                    ntohl(hdr->sequence),
                    payload_size,
                    written);
            }
        }
    }

    return 0;
}
