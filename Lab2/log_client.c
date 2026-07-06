#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_MSG 1024

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <fifo_path> <LEVEL> <message>\n", argv[0]);
        printf("Example: %s /tmp/fifo1 INFO \"Hello world\"\n", argv[0]);
        return 1;
    }

    char *fifo = argv[1];
    char *level = argv[2];
    char *message = argv[3];

    int fd = open(fifo, O_WRONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    char buffer[MAX_MSG];

    // Send multiple messages
    for (int i = 0; i < 5; i++) {

        snprintf(buffer, sizeof(buffer), "%s|%s (%d)\n", level, message, i+1);

        // Ensure message <= PIPE_BUF (safe atomic write)
        if (strlen(buffer) > 4096) {
            printf("Message too large! Must be <= PIPE_BUF\n");
            break;
        }

        write(fd, buffer, strlen(buffer));

        sleep(1); // simulate delay between logs
    }

    close(fd);

    return 0;
}
