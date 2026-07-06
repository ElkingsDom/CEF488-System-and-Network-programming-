#include "crypto.h"

#define XOR_KEY 0x5A

void xor_crypt(unsigned char *data,
               int len)
{
    int i;

    for(i = 0; i < len; i++)
    {
        data[i] ^= XOR_KEY;
    }
}
