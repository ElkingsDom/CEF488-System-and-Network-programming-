#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "protocol.h"

/*
 * send_all - Sends exactly 'length' bytes, retrying on partial sends.
 *
 * TCP does not guarantee that a single send() delivers all bytes.
 * This wrapper loops until all bytes are sent or an error occurs.
 */
ssize_t send_all(int sock,
                 const void *buffer,
                 size_t length)
{
    size_t total_sent = 0;

    const char *buf = buffer;

    while (total_sent < length)
    {
        ssize_t sent =
            send(sock,
                 buf + total_sent,
                 length - total_sent,
                 0);

        if (sent <= 0)
        {
            return -1;
        }

        total_sent += sent;
    }

    return total_sent;
}

/*
 * recv_all - Receives exactly 'length' bytes, retrying on partial reads.
 *
 * TCP may deliver data in smaller chunks than requested. This wrapper
 * loops until all expected bytes arrive or the connection is closed.
 */
ssize_t recv_all(int sock,
                 void *buffer,
                 size_t length)
{
    size_t total_received = 0;

    char *buf = buffer;

    while (total_received < length)
    {
        ssize_t received =
            recv(sock,
                 buf + total_received,
                 length - total_received,
                 0);

        if (received <= 0)
        {
            return -1;
        }

        total_received += received;
    }

    return total_received;
}

/*
 * send_task - Serialises and sends a task message to a worker.
 *
 * Message layout (network byte order):
 *   [4B opcode=WORK][4B width][4B height][4B start_row]
 *   [4B num_rows][4B max_iter][8B center_x][8B center_y][8B zoom]
 *
 * Integers use htonl(); doubles are sent as raw IEEE 754 bytes
 * (both master and workers run on Linux x86-64, same endianness).
 */
int send_task(int sock,
              struct task *t)
{
    uint32_t opcode =
        htonl(OPCODE_WORK);

    if (send_all(sock,
                 &opcode,
                 sizeof(opcode)) < 0)
    {
        return -1;
    }

    uint32_t data[5];

    data[0] = htonl(t->width);
    data[1] = htonl(t->height);
    data[2] = htonl(t->start_row);
    data[3] = htonl(t->num_rows);
    data[4] = htonl(t->max_iter);

    if (send_all(sock,
                 data,
                 sizeof(data)) < 0)
    {
        return -1;
    }

    if (send_all(sock,
                 &t->center_x,
                 sizeof(double)) < 0)
    {
        return -1;
    }

    if (send_all(sock,
                 &t->center_y,
                 sizeof(double)) < 0)
    {
        return -1;
    }

    if (send_all(sock,
                 &t->zoom,
                 sizeof(double)) < 0)
    {
        return -1;
    }

    return 0;
}

/*
 * recv_task - Receives and deserialises a task or terminate message.
 *
 * Returns:
 *   0  on a valid WORK task (t is populated),
 *   1  if a TERMINATE opcode is received (worker should exit),
 *  -1  on error or unexpected opcode.
 */
int recv_task(int sock,
              struct task *t)
{
    uint32_t opcode;

    if (recv_all(sock,
                 &opcode,
                 sizeof(opcode)) < 0)
    {
        return -1;
    }

    opcode = ntohl(opcode);

    /* Worker receives TERMINATE: signal clean exit to caller. */
    if (opcode == OPCODE_TERMINATE)
    {
        return 1;
    }

    if (opcode != OPCODE_WORK)
    {
        return -1;
    }

    uint32_t data[5];

    if (recv_all(sock,
                 data,
                 sizeof(data)) < 0)
    {
        return -1;
    }

    t->width    = ntohl(data[0]);
    t->height   = ntohl(data[1]);
    t->start_row= ntohl(data[2]);
    t->num_rows = ntohl(data[3]);
    t->max_iter = ntohl(data[4]);

    if (recv_all(sock,
                 &t->center_x,
                 sizeof(double)) < 0)
    {
        return -1;
    }

    if (recv_all(sock,
                 &t->center_y,
                 sizeof(double)) < 0)
    {
        return -1;
    }

    if (recv_all(sock,
                 &t->zoom,
                 sizeof(double)) < 0)
    {
        return -1;
    }

    return 0;
}

/*
 * send_result - Serialises and sends a result message to the master.
 *
 * Message layout:
 *   [4B opcode=RESULT][4B start_row][4B num_rows][4B width]
 *   [num_rows * width bytes of grayscale pixel data]
 */
int send_result(int sock,
                struct result *r)
{
    uint32_t opcode =
        htonl(OPCODE_RESULT);

    if (send_all(sock,
                 &opcode,
                 sizeof(opcode)) < 0)
    {
        return -1;
    }

    uint32_t header[3];

    header[0] = htonl(r->start_row);
    header[1] = htonl(r->num_rows);
    header[2] = htonl(r->width);

    if (send_all(sock,
                 header,
                 sizeof(header)) < 0)
    {
        return -1;
    }

    size_t pixel_bytes =
        r->num_rows * r->width;

    if (send_all(sock,
                 r->pixels,
                 pixel_bytes) < 0)
    {
        return -1;
    }

    return 0;
}

/*
 * recv_result - Receives and deserialises a result message.
 *
 * Allocates r->pixels on the heap; caller must free() it.
 */
int recv_result(int sock,
                struct result *r)
{
    uint32_t opcode;

    if (recv_all(sock,
                 &opcode,
                 sizeof(opcode)) < 0)
    {
        return -1;
    }

    opcode = ntohl(opcode);

    if (opcode != OPCODE_RESULT)
    {
        return -1;
    }

    uint32_t header[3];

    if (recv_all(sock,
                 header,
                 sizeof(header)) < 0)
    {
        return -1;
    }

    r->start_row = ntohl(header[0]);
    r->num_rows  = ntohl(header[1]);
    r->width     = ntohl(header[2]);

    size_t pixel_bytes =
        r->num_rows * r->width;

    r->pixels = malloc(pixel_bytes);

    if (!r->pixels)
    {
        return -1;
    }

    if (recv_all(sock,
                 r->pixels,
                 pixel_bytes) < 0)
    {
        free(r->pixels);
        return -1;
    }

    return 0;
}

/*
 * send_work_request - Worker sends an opcode asking for the next chunk.
 */
int send_work_request(int sock)
{
    uint32_t opcode =
        htonl(OPCODE_REQUEST);

    return send_all(sock,
                    &opcode,
                    sizeof(opcode));
}

/*
 * recv_work_request - Master receives a work request opcode from a worker.
 *
 * Returns 0 on success, -1 if the opcode is wrong or the socket failed.
 */
int recv_work_request(int sock)
{
    uint32_t opcode;

    if (recv_all(sock,
                 &opcode,
                 sizeof(opcode)) < 0)
    {
        return -1;
    }

    opcode = ntohl(opcode);

    if (opcode != OPCODE_REQUEST)
    {
        return -1;
    }

    return 0;
}

/*
 * send_terminate - Master tells a worker there is no more work.
 */
int send_terminate(int sock)
{
    uint32_t opcode =
        htonl(OPCODE_TERMINATE);

    return send_all(sock,
                    &opcode,
                    sizeof(opcode)) < 0 ? -1 : 0;
}
