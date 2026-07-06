#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_MSG 1024

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <fifo_path> <LEVEL|message>\n", argv[0]);
        return 1;
    }

    char *fifo = argv[1];
    char *message = argv[2];

    int fd = open(fifo, O_WRONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    char buffer[MAX_MSG];
    snprintf(buffer, sizeof(buffer), "%s\n", message);

    write(fd, buffer, strlen(buffer));

    close(fd);

    return 0;
}
