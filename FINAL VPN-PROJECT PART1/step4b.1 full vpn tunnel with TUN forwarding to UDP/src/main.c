#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "tun.h"
#include "udp.h"
#include "protocol.h"

#define PORT 5555
#define BUFFER_SIZE 2000

int main()
{
    char tun_name[20] = "tun0";

    int tun_fd;

    int sockfd;

    unsigned char packet[BUFFER_SIZE];

    vpn_header_t *hdr;

    uint32_t seq = 1;

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
        int n;

        n = read(tun_fd,
                 packet + sizeof(vpn_header_t),
                 BUFFER_SIZE - sizeof(vpn_header_t));

        if(n <= 0)
            continue;

        hdr = (vpn_header_t *)packet;

        hdr->command = CMD_DATA;
        hdr->sequence = htonl(seq);

        send_packet(sockfd,
                    "192.168.8.105",
                    PORT,
                    packet,
                    n + sizeof(vpn_header_t));

        printf("Sent packet seq=%u size=%d bytes\n",
               seq,
               n);

        seq++;
    }

    return 0;
}
