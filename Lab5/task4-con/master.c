#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"
#include "mandelbrot.h"

#define MAX_WORKERS 3

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr,
                "Usage: %s <port>\n",
                argv[0]);

        return 1;
    }

    int port = atoi(argv[1]);

    int server_fd =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if (server_fd < 0)
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

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(port);

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd,
               MAX_WORKERS) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Master listening on port %d...\n",
           port);

    int worker_sockets[MAX_WORKERS];

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        struct sockaddr_in worker_addr;

        socklen_t addr_len =
            sizeof(worker_addr);

        int worker_fd =
            accept(server_fd,
                   (struct sockaddr *)&worker_addr,
                   &addr_len);

        if (worker_fd < 0)
        {
            perror("accept");
            return 1;
        }

        worker_sockets[i] =
            worker_fd;

        printf("Worker %d connected.\n",
               i + 1);
    }

    int width = 800;
    int height = 600;

    unsigned char *image =
        malloc(width * height);

    if (!image)
    {
        perror("malloc");
        return 1;
    }

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        struct task t;

        t.width = width;
        t.height = height;

        t.start_row =
            i * (height / MAX_WORKERS);

        t.num_rows =
            height / MAX_WORKERS;

        t.max_iter = 1000;

        t.center_x = -0.5;
        t.center_y = 0.0;

        t.zoom = 0.005;

        printf("Assigning rows %u-%u to worker %d\n",
               t.start_row,
               t.start_row + t.num_rows - 1,
               i + 1);

        if (send_task(worker_sockets[i],
                      &t) < 0)
        {
            fprintf(stderr,
                    "Failed to send task.\n");
        }
    }

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        struct result r;

        if (recv_result(worker_sockets[i],
                        &r) < 0)
        {
            fprintf(stderr,
                    "Failed to receive result.\n");

            continue;
        }

        printf("Received rows %u-%u\n",
               r.start_row,
               r.start_row + r.num_rows - 1);

        memcpy(&image[r.start_row * width],
               r.pixels,
               r.num_rows * width);

        free(r.pixels);
    }

    write_ppm("distributed_mandelbrot.ppm",
              image,
              width,
              height);

    printf("\nDistributed image generated successfully.\n");

    free(image);

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        close(worker_sockets[i]);
    }

    close(server_fd);

    return 0;
}
