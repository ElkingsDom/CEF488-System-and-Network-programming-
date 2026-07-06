#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

int running = 1;

void handle_sigint(int sig) {
    running = 0;
}

// Create FIFO safely
void create_fifo(const char *path) {
    if (mkfifo(path, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
    }
}

int main() {
    signal(SIGINT, handle_sigint);

    // STEP 1: Create FIFOs
    create_fifo("/tmp/fifo1");
    create_fifo("/tmp/fifo2");
    create_fifo("/tmp/fifo3");
    create_fifo("/tmp/alert_fifo");

    // STEP 2: Open FIFOs (read, non-blocking)
    int fd1 = open("/tmp/fifo1", O_RDONLY | O_NONBLOCK);
    int fd2 = open("/tmp/fifo2", O_RDONLY | O_NONBLOCK);
    int fd3 = open("/tmp/fifo3", O_RDONLY | O_NONBLOCK);

    if (fd1 < 0 || fd2 < 0 || fd3 < 0) {
        perror("open fifo");
        exit(EXIT_FAILURE);
    }

    // STEP 3: Open alert FIFO (write)
    int alert_fd = open("/tmp/alert_fifo", O_RDWR | O_NONBLOCK);
    if (alert_fd < 0) {
        perror("open alert fifo");
    }

    // STEP 4: Create epoll
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev, events[MAX_EVENTS];

    // Add all FIFOs to epoll
    int fds[] = {fd1, fd2, fd3};

    for (int i = 0; i < 3; i++) {
        ev.events = EPOLLIN | EPOLLET; // edge-triggered
        ev.data.fd = fds[i];

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, fds[i], &ev) == -1) {
            perror("epoll_ctl");
            exit(EXIT_FAILURE);
        }
    }

    char buffer[BUFFER_SIZE];

    printf("Server running... waiting for logs\n");

    // STEP 5: Main loop
    while (running) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, 1000);

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            // IMPORTANT: Read until EAGAIN (edge-triggered rule)
            while (1) {
                ssize_t count = read(fd, buffer, BUFFER_SIZE - 1);

                if (count == -1) {
                    if (errno == EAGAIN)
                        break;
                    perror("read");
                    break;
                } 
                else if (count == 0) {
                    // FIFO closed
                    close(fd);
                    break;
                } 
                else {
                    buffer[count] = '\0';

                    // STEP 6: Parse LEVEL|message
                    char *level = strtok(buffer, "|");
                    char *msg = strtok(NULL, "\n");

                    // Timestamp
                    time_t now = time(NULL);

                    char *time_str = ctime(&now);
time_str[strlen(time_str) - 1] = '\0'; // remove newline

printf("[%s] %s | %s\n", time_str, level, msg);

                    // STEP 7: Handle CRITICAL logs
                    if (level && strcmp(level, "CRITICAL") == 0) {
                        write(alert_fd, buffer, strlen(buffer));
                    }
                }
            }
        }
    }

    // STEP 8: Cleanup
    close(fd1);
    close(fd2);
    close(fd3);
    close(alert_fd);

    unlink("/tmp/fifo1");
    unlink("/tmp/fifo2");
    unlink("/tmp/fifo3");
    unlink("/tmp/alert_fifo");

    printf("Server stopped.\n");

    return 0;
}
