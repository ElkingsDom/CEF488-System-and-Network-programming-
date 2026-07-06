#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"
#include "mandelbrot.h"

/*
 * Task 5 - Dynamic worker
 * ========================
 * The worker connects to the master and enters a request/compute/send
 * loop:
 *   1. Send a WORK_REQUEST opcode.
 *   2. Receive a task (WORK) or a TERMINATE message.
 *   3. If WORK: compute all assigned rows and send a RESULT back.
 *   4. If TERMINATE: exit cleanly.
 *
 * recv_task() returns:
 *    0  - valid task received
 *    1  - TERMINATE opcode received
 *   -1  - error
 */
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
    int   port      = atoi(argv[2]);

    /* --- Connect to master ---------------------------------------- */
    int sock =
        socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);

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

    printf("Connected to master at %s:%d\n",
           server_ip, port);

    /* --- Main work loop ------------------------------------------- */
    while (1)
    {
        /* 1. Request next work chunk from master */
        if (send_work_request(sock) < 0)
        {
            fprintf(stderr,
                    "Failed to send work request.\n");
            break;
        }

        /* 2. Receive task or termination signal */
        struct task t;

        int rc = recv_task(sock, &t);

        if (rc == 1)
        {
            /* TERMINATE: master has no more chunks for us */
            printf("Termination received. Exiting.\n");
            break;
        }

        if (rc < 0)
        {
            fprintf(stderr,
                    "Failed to receive task.\n");
            break;
        }

        printf("Computing rows %u-%u (width=%u)...\n",
               t.start_row,
               t.start_row + t.num_rows - 1,
               t.width);

        /* 3. Allocate pixel buffer and compute each row */
        size_t pixel_bytes =
            t.num_rows * t.width;

        unsigned char *pixels =
            malloc(pixel_bytes);

        if (!pixels)
        {
            perror("malloc");
            break;
        }

        for (uint32_t row = 0;
             row < t.num_rows;
             row++)
        {
            compute_row(
                &pixels[row * t.width],
                (int)(t.start_row + row),
                (int)t.width,
                (int)t.height,
                t.center_x,
                t.center_y,
                t.zoom,
                (int)t.max_iter);
        }

        /* 4. Send result back to master */
        struct result r;
        r.start_row = t.start_row;
        r.num_rows  = t.num_rows;
        r.width     = t.width;
        r.pixels    = pixels;

        if (send_result(sock, &r) < 0)
        {
            fprintf(stderr,
                    "Failed to send result for rows %u-%u.\n",
                    t.start_row,
                    t.start_row + t.num_rows - 1);

            free(pixels);
            break;
        }

        printf("Sent result for rows %u-%u.\n",
               t.start_row,
               t.start_row + t.num_rows - 1);

        free(pixels);
    }

    close(sock);

    return 0;
}
