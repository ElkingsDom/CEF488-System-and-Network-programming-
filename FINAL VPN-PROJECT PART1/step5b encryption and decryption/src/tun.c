#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <linux/if.h>
#include <linux/if_tun.h>

#include "tun.h"

int tun_alloc(char *dev)
{
    struct ifreq ifr;
    int fd;

    fd = open("/dev/net/tun", O_RDWR);

    if(fd < 0)
    {
        perror("open");
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if(*dev)
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    if(ioctl(fd, TUNSETIFF, (void *)&ifr) < 0)
    {
        perror("ioctl");
        close(fd);
        return -1;
    }

    strcpy(dev, ifr.ifr_name);

    return fd;
}
