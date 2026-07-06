#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "protocol.h"
#include "mandelbrot.h"

/*
 * Task 5 - Dynamic Load Balancing Master
 * =======================================
 * The master maintains a queue of row chunks.  Workers connect,
 * request work, process a chunk, return the result, and repeat.
 * This automatically balances load: faster workers receive more
 * chunks.  When all rows are dispatched, workers are told to
 * terminate.
 *
 * Fault tolerance (Task 2 requirement):
 *   select() with a 5-second timeout detects unresponsive workers.
 *   If a worker times out, its socket is closed and its in-flight
 *   chunk is pushed back onto the queue so another worker finishes it.
 *
 * Task 6 - Performance analysis:
 *   The master records wall-clock time for the distributed render
 *   and prints it at the end so it can be compared with the local
 *   test_render time.
 */

#define MAX_WORKERS    3
#define CHUNK_SIZE     10   /* rows per work unit */
#define WORKER_TIMEOUT 5    /* seconds before a worker is declared dead */

/* Per-worker state ------------------------------------------------- */
typedef struct
{
    int      sock;          /* -1 = slot unused / worker dead          */
    int      active;        /* 1 while we are waiting for a result      */
    uint32_t chunk_start;   /* start row of the in-flight chunk         */
    uint32_t chunk_rows;    /* number of rows in the in-flight chunk    */
} Worker;

/* Simple queue of row ranges yet to be assigned -------------------- */
typedef struct
{
    uint32_t start;
    uint32_t num_rows;
} Chunk;

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr,
                "Usage: %s <port>\n",
                argv[0]);
        return 1;
    }

    /* --- Image parameters ----------------------------------------- */
    const int    width    = 800;
    const int    height   = 600;
    const int    max_iter = 1000;
    const double center_x = -0.5;
    const double center_y =  0.0;
    const double zoom     =  0.005;

    int port = atoi(argv[1]);

    /* --- Build chunk queue ---------------------------------------- */
    int total_chunks =
        (height + CHUNK_SIZE - 1) / CHUNK_SIZE;

    Chunk *queue =
        malloc(total_chunks * sizeof(Chunk));

    if (!queue)
    {
        perror("malloc queue");
        return 1;
    }

    for (int i = 0; i < total_chunks; i++)
    {
        queue[i].start = i * CHUNK_SIZE;

        uint32_t end =
            queue[i].start + CHUNK_SIZE;

        queue[i].num_rows =
            (end <= (uint32_t)height)
                ? CHUNK_SIZE
                : (uint32_t)height - queue[i].start;
    }

    int queue_head = 0;     /* next chunk to dispatch  */
    int queue_tail = total_chunks; /* one past last valid idx */

    /* --- TCP server setup ----------------------------------------- */
    int server =
        socket(AF_INET, SOCK_STREAM, 0);

    if (server < 0)
    {
        perror("socket");
        free(queue);
        return 1;
    }

    int opt = 1;
    setsockopt(server,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0)
    {
        perror("bind");
        close(server);
        free(queue);
        return 1;
    }

    listen(server, MAX_WORKERS);

    printf("Master listening on port %d\n", port);
    printf("Image: %dx%d  chunks: %d  chunk_size: %d\n",
           width, height, total_chunks, CHUNK_SIZE);
    printf("Waiting for up to %d workers...\n\n",
           MAX_WORKERS);

    /* --- Accept workers ------------------------------------------- */
    Worker workers[MAX_WORKERS];

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        workers[i].sock        = -1;
        workers[i].active      =  0;
        workers[i].chunk_start =  0;
        workers[i].chunk_rows  =  0;
    }

    for (int i = 0; i < MAX_WORKERS; i++)
    {
        workers[i].sock =
            accept(server, NULL, NULL);

        if (workers[i].sock < 0)
        {
            perror("accept");
            continue;
        }

        printf("Worker %d connected (fd=%d)\n",
               i + 1,
               workers[i].sock);
    }

    /* --- Image buffer --------------------------------------------- */
    unsigned char *image =
        malloc(width * height);

    if (!image)
    {
        perror("malloc image");
        free(queue);
        return 1;
    }

    memset(image, 0, width * height);

    uint32_t completed_rows = 0;

    /* --- Start performance timer ---------------------------------- */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    printf("Starting dynamic distribution...\n\n");

    /*
     * Main dispatch loop
     * ------------------
     * Repeat until every row has been received:
     *   1. For each idle worker, send the next chunk (or TERMINATE).
     *   2. Use select() with a timeout to wait for any result.
     *   3. Receive the result; if a worker times out, re-queue its chunk.
     */
    while (completed_rows < (uint32_t)height)
    {
        /* Step 1: send work to every idle worker that has a free slot */
        for (int i = 0; i < MAX_WORKERS; i++)
        {
            if (workers[i].sock < 0 ||
                workers[i].active)
            {
                continue; /* dead or already busy */
            }

            /* Wait for the worker's REQUEST opcode */
            if (recv_work_request(workers[i].sock) < 0)
            {
                fprintf(stderr,
                        "Worker %d: failed to receive request, "
                        "marking dead.\n",
                        i + 1);

                /* Re-queue any in-flight chunk (shouldn't be active here,
                   but guard anyway). */
                close(workers[i].sock);
                workers[i].sock = -1;
                continue;
            }

            /* Are there chunks left to send? */
            if (queue_head >= queue_tail)
            {
                /* No more work: terminate this worker. */
                send_terminate(workers[i].sock);
                close(workers[i].sock);
                workers[i].sock = -1;
                printf("Worker %d: no more work, terminated.\n",
                       i + 1);
                continue;
            }

            /* Dispatch the next chunk */
            Chunk *c = &queue[queue_head++];

            struct task t;
            t.width     = width;
            t.height    = height;
            t.start_row = c->start;
            t.num_rows  = c->num_rows;
            t.max_iter  = max_iter;
            t.center_x  = center_x;
            t.center_y  = center_y;
            t.zoom      = zoom;

            if (send_task(workers[i].sock, &t) < 0)
            {
                fprintf(stderr,
                        "Worker %d: failed to send task, "
                        "re-queuing chunk %u.\n",
                        i + 1,
                        c->start);

                /* Push chunk back by rewinding head */
                queue_head--;

                close(workers[i].sock);
                workers[i].sock = -1;
                continue;
            }

            workers[i].active      = 1;
            workers[i].chunk_start = c->start;
            workers[i].chunk_rows  = c->num_rows;

            printf("Worker %d: assigned rows %u-%u\n",
                   i + 1,
                   c->start,
                   c->start + c->num_rows - 1);
        }

        /*
         * Step 2: select() - wait for any active worker to respond,
         *         with a WORKER_TIMEOUT second deadline.
         */
        fd_set read_fds;
        FD_ZERO(&read_fds);

        int max_fd = -1;
        int any_active = 0;

        for (int i = 0; i < MAX_WORKERS; i++)
        {
            if (workers[i].sock >= 0 &&
                workers[i].active)
            {
                FD_SET(workers[i].sock, &read_fds);

                if (workers[i].sock > max_fd)
                    max_fd = workers[i].sock;

                any_active = 1;
            }
        }

        if (!any_active)
        {
            /*
             * No active workers and rows still missing means all workers
             * died.  Print an error and abort.
             */
            fprintf(stderr,
                    "All workers dead but %u rows unfinished. "
                    "Aborting.\n",
                    (uint32_t)height - completed_rows);
            break;
        }

        struct timeval timeout;
        timeout.tv_sec  = WORKER_TIMEOUT;
        timeout.tv_usec = 0;

        int ready =
            select(max_fd + 1,
                   &read_fds,
                   NULL,
                   NULL,
                   &timeout);

        if (ready < 0)
        {
            perror("select");
            break;
        }

        if (ready == 0)
        {
            /* Timeout: find which workers are overdue and kill them. */
            fprintf(stderr,
                    "Timeout! Checking for dead workers...\n");

            for (int i = 0; i < MAX_WORKERS; i++)
            {
                if (workers[i].sock >= 0 &&
                    workers[i].active)
                {
                    fprintf(stderr,
                            "Worker %d timed out; re-queuing "
                            "rows %u-%u.\n",
                            i + 1,
                            workers[i].chunk_start,
                            workers[i].chunk_start +
                                workers[i].chunk_rows - 1);

                    /*
                     * Re-queue the lost chunk by inserting it at the
                     * current queue_head position (rewind one slot).
                     * Since queue_head already advanced past this chunk,
                     * we write it back and decrement.
                     */
                    queue_head--;
                    queue[queue_head].start    = workers[i].chunk_start;
                    queue[queue_head].num_rows = workers[i].chunk_rows;

                    close(workers[i].sock);
                    workers[i].sock   = -1;
                    workers[i].active =  0;
                }
            }

            continue; /* retry with surviving/reconnected workers */
        }

        /* Step 3: collect results from all ready sockets */
        for (int i = 0; i < MAX_WORKERS; i++)
        {
            if (workers[i].sock < 0 ||
                !workers[i].active ||
                !FD_ISSET(workers[i].sock, &read_fds))
            {
                continue;
            }

            struct result r;

            if (recv_result(workers[i].sock, &r) < 0)
            {
                fprintf(stderr,
                        "Worker %d: recv_result failed, "
                        "re-queuing rows %u-%u.\n",
                        i + 1,
                        workers[i].chunk_start,
                        workers[i].chunk_start +
                            workers[i].chunk_rows - 1);

                queue_head--;
                queue[queue_head].start    = workers[i].chunk_start;
                queue[queue_head].num_rows = workers[i].chunk_rows;

                close(workers[i].sock);
                workers[i].sock   = -1;
                workers[i].active =  0;
                continue;
            }

            /* Copy pixel data into the global image buffer */
            memcpy(&image[r.start_row * width],
                   r.pixels,
                   r.num_rows * width);

            completed_rows += r.num_rows;

            printf("Worker %d: received rows %u-%u  "
                   "(%u/%d rows done)\n",
                   i + 1,
                   r.start_row,
                   r.start_row + r.num_rows - 1,
                   completed_rows,
                   height);

            free(r.pixels);

            workers[i].active = 0; /* ready for next chunk */
        }
    }

    /* --- Stop timer and compute performance metrics --------------- */
    clock_gettime(CLOCK_MONOTONIC, &t_end);

    double elapsed =
        (t_end.tv_sec  - t_start.tv_sec) +
        (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    printf("\n=== Performance (Task 6) ===\n");
    printf("Distributed render time : %.3f seconds\n", elapsed);
    printf("Workers used            : %d\n", MAX_WORKERS);
    printf("Rows completed          : %u / %d\n",
           completed_rows, height);
    printf("To compute speedup, run: ./test_render\n");
    printf("  speedup = local_time / %.3f\n", elapsed);
    printf("============================\n\n");

    /* --- Save image ----------------------------------------------- */
    if (completed_rows == (uint32_t)height)
    {
        write_ppm("dynamic_mandelbrot.ppm",
                  image,
                  width,
                  height);

        printf("Image saved to dynamic_mandelbrot.ppm\n");
    }
    else
    {
        fprintf(stderr,
                "Warning: image incomplete (%u/%d rows).\n",
                completed_rows,
                height);
    }

    /* --- Cleanup -------------------------------------------------- */
    for (int i = 0; i < MAX_WORKERS; i++)
    {
        if (workers[i].sock >= 0)
            close(workers[i].sock);
    }

    close(server);
    free(image);
    free(queue);

    return 0;
}
