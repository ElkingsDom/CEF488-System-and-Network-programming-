#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#define MAX_EVENTS 20
#define BUFFER_SIZE 1024

int running = 1;
int event_fd;

// Structure to pass FIFO info to thread
typedef struct {
    int fd;
} fifo_thread_arg;

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

// Thread: waits for data, notifies main thread
void *fifo_thread(void *arg) {
    fifo_thread_arg *data = (fifo_thread_arg *)arg;
    char buffer[BUFFER_SIZE];

    while (running) {
        ssize_t count = read(data->fd, buffer, BUFFER_SIZE);

        if (count > 0) {
            uint64_t val = data->fd; // send fd as signal
            write(event_fd, &val, sizeof(val));
        }
    }

    return NULL;
}

int main() {
    signal(SIGINT, handle_sigint);

    // =========================
    // CREATE FIFOS
    // =========================
    create_fifo("/tmp/fifo1");
    create_fifo("/tmp/fifo2");
    create_fifo("/tmp/fifo3");

    int fd1 = open("/tmp/fifo1", O_RDONLY | O_NONBLOCK);
    int fd2 = open("/tmp/fifo2", O_RDONLY | O_NONBLOCK);
    int fd3 = open("/tmp/fifo3", O_RDONLY | O_NONBLOCK);

    if (fd1 < 0 || fd2 < 0 || fd3 < 0) {
        perror("open fifo");
        exit(EXIT_FAILURE);
    }

    // =========================
    // CREATE EVENTFD
    // =========================
    event_fd = eventfd(0, EFD_NONBLOCK);
    if (event_fd == -1) {
        perror("eventfd");
        exit(EXIT_FAILURE);
    }

    // =========================
    // EPOLL SETUP
    // =========================
    int epfd = epoll_create1(0);
    struct epoll_event ev, events[MAX_EVENTS];

    ev.events = EPOLLIN;
    ev.data.fd = event_fd;

    epoll_ctl(epfd, EPOLL_CTL_ADD, event_fd, &ev);

    // =========================
    // CREATE THREADS FOR FIFOS
    // =========================
    pthread_t t1, t2, t3;

    fifo_thread_arg a1 = {fd1};
    fifo_thread_arg a2 = {fd2};
    fifo_thread_arg a3 = {fd3};

    pthread_create(&t1, NULL, fifo_thread, &a1);
    pthread_create(&t2, NULL, fifo_thread, &a2);
    pthread_create(&t3, NULL, fifo_thread, &a3);

    char buffer[BUFFER_SIZE];

    printf("Server running (eventfd mode)...\n");

    // =========================
    // MAIN LOOP
    // =========================
    while (running) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, 1000);

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == event_fd) {

                uint64_t val;
                read(event_fd, &val, sizeof(val));

                int fd = (int)val;

                ssize_t count = read(fd, buffer, BUFFER_SIZE - 1);
                if (count > 0) {
                    buffer[count] = '\0';

                    char *level = strtok(buffer, "|");
                    char *msg = strtok(NULL, "\n");

                    time_t now = time(NULL);
                    char *t = ctime(&now);
                    t[strlen(t) - 1] = '\0';

                    printf("[%s] %s | %s\n", t, level, msg);
                }
            }
        }
    }

    close(fd1);
    close(fd2);
    close(fd3);
    close(event_fd);

    printf("Server stopped.\n");
    return 0;
}
