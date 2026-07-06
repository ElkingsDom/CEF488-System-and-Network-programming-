#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define CMD_DATA 1
#define CMD_KEEPALIVE 2

typedef struct
{
    uint8_t command;
    uint32_t sequence;
} vpn_header_t;

#endif
