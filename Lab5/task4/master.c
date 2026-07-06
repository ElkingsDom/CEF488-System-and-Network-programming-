#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"

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
        socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;

    memset(&server_addr, 0,
           sizeof(server_addr));

    server_addr.sin_family = AF_INET;

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
            continue;
        }

        worker_sockets[i] =
            worker_fd;

        printf("Worker %d connected from %s:%d\n",
               i + 1,
               inet_ntoa(worker_addr.sin_addr),
               ntohs(worker_addr.sin_port));
    }

    printf("All workers connected.\n");

    int width = 800;
    int height = 600;

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

        printf("Sending rows %u-%u to worker %d\n",
               t.start_row,
               t.start_row + t.num_rows - 1,
               i + 1);

        if (send_task(worker_sockets[i],
                      &t) < 0)
        {
            perror("send_task");
        }
    }

    sleep(3);

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        close(worker_sockets[i]);
    }

    close(server_fd);

    return 0;
}
