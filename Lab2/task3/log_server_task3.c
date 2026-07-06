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

#define MAX_EVENTS 20
#define BUFFER_SIZE 1024

int running = 1;

void handle_sigint(int sig) {
    (void)sig;
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

    // =========================
    // STEP 1: CREATE FIFOS
    // =========================
    create_fifo("/tmp/fifo1");
    create_fifo("/tmp/fifo2");
    create_fifo("/tmp/fifo3");
    create_fifo("/tmp/alert_fifo");
    create_fifo("/tmp/control");

    // =========================
    // STEP 2: OPEN FIFOS
    // =========================
    int fd1 = open("/tmp/fifo1", O_RDONLY | O_NONBLOCK);
    int fd2 = open("/tmp/fifo2", O_RDONLY | O_NONBLOCK);
    int fd3 = open("/tmp/fifo3", O_RDONLY | O_NONBLOCK);

    if (fd1 < 0 || fd2 < 0 || fd3 < 0) {
        perror("open fifo");
        exit(EXIT_FAILURE);
    }

    // Alert FIFO (use RDWR to avoid blocking)
    int alert_fd = open("/tmp/alert_fifo", O_RDWR | O_NONBLOCK);
    if (alert_fd < 0) {
        perror("open alert fifo");
        exit(EXIT_FAILURE);
    }

    // Control FIFO
    int control_fd = open("/tmp/control", O_RDONLY | O_NONBLOCK);
    if (control_fd < 0) {
        perror("open control fifo");
        exit(EXIT_FAILURE);
    }

    // =========================
    // STEP 3: EPOLL SETUP
    // =========================
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev, events[MAX_EVENTS];

    // Add initial FIFOs
    int fds[] = {fd1, fd2, fd3};
    for (int i = 0; i < 3; i++) {
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fds[i];

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, fds[i], &ev) == -1) {
            perror("epoll_ctl fifo");
            exit(EXIT_FAILURE);
        }
    }

    // Add control FIFO
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = control_fd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, control_fd, &ev) == -1) {
        perror("epoll_ctl control");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];

    printf("Server running... waiting for logs\n");

    // =========================
    // MAIN LOOP
    // =========================
    while (running) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, 1000);

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            // =========================
            // HANDLE CONTROL FIFO
            // =========================
            if (fd == control_fd) {
                while (1) {
                    ssize_t count = read(fd, buffer, BUFFER_SIZE - 1);

                    if (count == -1) {
                        if (errno == EAGAIN)
                            break;
                        perror("read control");
                        break;
                    } 
                    else if (count == 0) {
                        break;
                    } 
                    else {
                        buffer[count] = '\0';

                        // Expect: ADD /tmp/log_xxxx
                        if (strncmp(buffer, "ADD", 3) == 0) {
                            char *path = buffer + 4;
                            path[strcspn(path, "\n")] = '\0';

                            printf("Adding new FIFO: %s\n", path);

                            int newfd = open(path, O_RDONLY | O_NONBLOCK);
                            if (newfd == -1) {
                                perror("open new fifo");
                                continue;
                            }

                            struct epoll_event new_ev;
                            new_ev.events = EPOLLIN | EPOLLET;
                            new_ev.data.fd = newfd;

                            if (epoll_ctl(epfd, EPOLL_CTL_ADD, newfd, &new_ev) == -1) {
                                perror("epoll_ctl new fifo");
                            }
                        }
                    }
                }
                continue;
            }

            // =========================
            // HANDLE LOG FIFOS
            // =========================
            while (1) {
                ssize_t count = read(fd, buffer, BUFFER_SIZE - 1);

                if (count == -1) {
                    if (errno == EAGAIN)
                        break;
                    perror("read");
                    break;
                } 
                else if (count == 0) {
                    printf("FIFO closed, removing...\n");
                    close(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    break;
                } 
                else {
                    buffer[count] = '\0';

                    // Save original message
                    char original[BUFFER_SIZE];
                    strcpy(original, buffer);

                    // Parse message
                    char *level = strtok(buffer, "|");
                    char *msg = strtok(NULL, "\n");

                    // Timestamp
                    time_t now = time(NULL);
                    char *time_str = ctime(&now);
                    time_str[strlen(time_str) - 1] = '\0';

                    printf("[%s] %s | %s\n", time_str, level, msg);

                    // Handle CRITICAL logs
                    if (level && strcmp(level, "CRITICAL") == 0) {
                        char alert_msg[BUFFER_SIZE];
                        snprintf(alert_msg, sizeof(alert_msg), "%s\n", original);
                        write(alert_fd, alert_msg, strlen(alert_msg));
                    }
                }
            }
        }
    }

    // =========================
    // CLEANUP
    // =========================
    close(fd1);
    close(fd2);
    close(fd3);
    close(alert_fd);
    close(control_fd);

    unlink("/tmp/fifo1");
    unlink("/tmp/fifo2");
    unlink("/tmp/fifo3");
    unlink("/tmp/alert_fifo");
    unlink("/tmp/control");

    printf("Server stopped.\n");

    return 0;
}
