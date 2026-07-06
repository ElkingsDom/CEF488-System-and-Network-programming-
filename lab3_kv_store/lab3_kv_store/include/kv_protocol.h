#ifndef KV_PROTOCOL_H
#define KV_PROTOCOL_H

#include <stdint.h>

#define PROTOCOL_VERSION 1

/* Maximum size bounds */
#define MAX_KEY_SIZE   256
#define MAX_VAL_SIZE   1024
#define MAX_PAYLOAD    (MAX_KEY_SIZE + MAX_VAL_SIZE + 2)

/* Request Opcodes */
#define OP_SET 1
#define OP_GET 2
#define OP_DEL 3

/* Response Status Codes */
#define STATUS_SUCCESS   0
#define STATUS_NOT_FOUND 1
#define STATUS_ERROR     2

/* Fixed-size 8-byte binary header */
struct kv_header {
    uint16_t version;  
    uint16_t opcode;   
    uint32_t length;   
} __attribute__((packed));

#endif /* KV_PROTOCOL_H */
