#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <sys/types.h>

#define OPCODE_WORK       1
#define OPCODE_TERMINATE  2
#define OPCODE_RESULT     3

struct task
{
    uint32_t width;
    uint32_t height;

    uint32_t start_row;
    uint32_t num_rows;

    uint32_t max_iter;

    double center_x;
    double center_y;

    double zoom;
};

struct result
{
    uint32_t start_row;
    uint32_t num_rows;

    uint32_t width;

    unsigned char *pixels;
};

ssize_t send_all(int sock,
                 const void *buffer,
                 size_t length);

ssize_t recv_all(int sock,
                 void *buffer,
                 size_t length);

int send_task(int sock,
              struct task *t);

int recv_task(int sock,
              struct task *t);

int send_result(int sock,
                struct result *r);

int recv_result(int sock,
                struct result *r);

#endif
