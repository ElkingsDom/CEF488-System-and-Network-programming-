#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

// -------- DAEMONIZE --------
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    chdir("/");
    umask(0);

    for (int i = sysconf(_SC_OPEN_MAX); i >= 0; i--)
        close(i);

    int fd = open("/dev/null", O_RDWR);
    dup(fd); dup(fd); dup(fd);
}

int main() {
    int pipe1[2], pipe2[2];
    pid_t child_pid;

    daemonize();
    openlog("task1_daemon", LOG_PID, LOG_DAEMON);

    if (pipe(pipe1) < 0 || pipe(pipe2) < 0) {
        syslog(LOG_ERR, "Pipe creation failed");
        exit(EXIT_FAILURE);
    }

    child_pid = fork();
    if (child_pid < 0) {
        syslog(LOG_ERR, "Fork failed");
        exit(EXIT_FAILURE);
    }

    // -------- CHILD --------
    if (child_pid == 0) {
        close(pipe1[1]);
        close(pipe2[0]);

        int sum = 0, num;

        while (read(pipe1[0], &num, sizeof(num)) > 0) {
            sum += num;
            write(pipe2[1], &sum, sizeof(sum));
        }

        exit(EXIT_SUCCESS);
    }

    // -------- PARENT --------
    close(pipe1[0]);
    close(pipe2[1]);

    int num = 1;

    for (int i = 0; i < 10; i++) {
        write(pipe1[1], &num, sizeof(num));
        sleep(1);

        int sum;
        if (read(pipe2[0], &sum, sizeof(sum)) > 0) {
            syslog(LOG_INFO, "Sum: %d", sum);
        }

        num++;
    }

    closelog();
    return 0;
}
