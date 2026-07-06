#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "protocol.h"

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

    t->width =
        ntohl(data[0]);

    t->height =
        ntohl(data[1]);

    t->start_row =
        ntohl(data[2]);

    t->num_rows =
        ntohl(data[3]);

    t->max_iter =
        ntohl(data[4]);

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

    header[0] =
        htonl(r->start_row);

    header[1] =
        htonl(r->num_rows);

    header[2] =
        htonl(r->width);

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

    r->start_row =
        ntohl(header[0]);

    r->num_rows =
        ntohl(header[1]);

    r->width =
        ntohl(header[2]);

    size_t pixel_bytes =
        r->num_rows * r->width;

    r->pixels =
        malloc(pixel_bytes);

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
