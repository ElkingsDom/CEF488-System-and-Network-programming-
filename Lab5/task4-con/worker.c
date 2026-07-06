#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"
#include "mandelbrot.h"

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

    if (connect(sock,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock);
        return 1;
    }

    printf("Connected to master.\n");

    struct task t;

    if (recv_task(sock,
                  &t) < 0)
    {
        fprintf(stderr,
                "Failed to receive task.\n");

        close(sock);
        return 1;
    }

    printf("Received rows %u-%u\n",
           t.start_row,
           t.start_row + t.num_rows - 1);

    size_t pixel_bytes =
        t.num_rows * t.width;

    unsigned char *pixels =
        malloc(pixel_bytes);

    if (!pixels)
    {
        perror("malloc");
        close(sock);
        return 1;
    }

    for (uint32_t row = 0;
         row < t.num_rows;
         row++)
    {
        compute_row(&pixels[row * t.width],
                    t.start_row + row,
                    t.width,
                    t.height,
                    t.center_x,
                    t.center_y,
                    t.zoom,
                    t.max_iter);
    }

    struct result r;

    r.start_row =
        t.start_row;

    r.num_rows =
        t.num_rows;

    r.width =
        t.width;

    r.pixels =
        pixels;

    printf("Sending computed rows back...\n");

    if (send_result(sock,
                    &r) < 0)
    {
        fprintf(stderr,
                "Failed to send result.\n");
    }

    free(pixels);

    close(sock);

    return 0;
}
