#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr,
                "Usage: %s <server_ip> <port>\n",
                argv[0]);

        return 1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);

    int sock =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;

    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons(port);

    if (inet_pton(AF_INET,
                  server_ip,
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");

        close(sock);
        return 1;
    }

    printf("Connecting to master %s:%d...\n",
           server_ip,
           port);

    if (connect(sock,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");

        close(sock);
        return 1;
    }

    printf("Connected to master successfully.\n");

    struct task t;

    if (recv_task(sock,
                  &t) < 0)
    {
        fprintf(stderr,
                "Failed to receive task.\n");

        close(sock);
        return 1;
    }

    printf("\n=== TASK RECEIVED ===\n");

    printf("Width      : %u\n",
           t.width);

    printf("Height     : %u\n",
           t.height);

    printf("Start Row  : %u\n",
           t.start_row);

    printf("Num Rows   : %u\n",
           t.num_rows);

    printf("Max Iter   : %u\n",
           t.max_iter);

    printf("Center X   : %f\n",
           t.center_x);

    printf("Center Y   : %f\n",
           t.center_y);

    printf("Zoom       : %f\n",
           t.zoom);

    close(sock);

    return 0;
}
