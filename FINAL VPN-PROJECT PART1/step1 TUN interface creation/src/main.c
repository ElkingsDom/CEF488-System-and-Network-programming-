#include <stdio.h>
#include <unistd.h>

#include "tun.h"

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

    printf("Press Ctrl+C to exit...\n");

    while(1)
    {
        sleep(1);
    }

    return 0;
}
