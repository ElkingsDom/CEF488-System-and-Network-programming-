#include <stdio.h>
#include <unistd.h>

#include "tun.h"

#define BUFFER_SIZE 2000

int main()
{
    char tun_name[20] = "tun0";

    int tun_fd = tun_alloc(tun_name);

    if(tun_fd < 0)
    {
        printf("Failed to create TUN interface\n");
        return 1;
    }

    printf("Created interface: %s\n", tun_name);

    unsigned char buffer[BUFFER_SIZE];

    while(1)
    {
        int n = read(tun_fd, buffer, sizeof(buffer));

        if(n > 0)
        {
            printf("Received packet (%d bytes)\n", n);
        }
    }

    return 0;
}
