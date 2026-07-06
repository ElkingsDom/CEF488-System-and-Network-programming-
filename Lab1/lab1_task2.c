#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

volatile sig_atomic_t running = 1;

// -------- SIGNALS --------
void handle_term(int sig) {
    (void)sig;
    running = 0;
}

void handle_sigchld(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// -------- DAEMON --------
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    setsid();
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
    openlog("task2_daemon", LOG_PID, LOG_DAEMON);

    // -------- SIGNAL SETUP --------
    struct sigaction sa;

    sa.sa_handler = handle_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &sa, NULL);

    // -------- PID FILE --------
    FILE *fp = fopen("/tmp/task2.pid", "w");
    if (fp) {
        fprintf(fp, "%d\n", getpid());
        fclose(fp);
    }

    pipe(pipe1);
    pipe(pipe2);

    child_pid = fork();

    if (child_pid == 0) {
        close(pipe1[1]);
        close(pipe2[0]);

        int sum = 0, num;

        while (1) {
            int r = read(pipe1[0], &num, sizeof(num));

            if (r <= 0) break;

            sum += num;
            write(pipe2[1], &sum, sizeof(sum));
        }

        exit(EXIT_SUCCESS);
    }

    close(pipe1[0]);
    close(pipe2[1]);

    int num = 1;

    while (running) {
        write(pipe1[1], &num, sizeof(num));
        sleep(1);

        int sum;
        if (read(pipe2[0], &sum, sizeof(sum)) > 0) {
            syslog(LOG_INFO, "Sum: %d", sum);
        }

        num++;
    }

    // -------- CLEAN SHUTDOWN --------
    syslog(LOG_INFO, "Stopping daemon...");

    close(pipe1[1]);  // VERY IMPORTANT → forces child to exit
    close(pipe2[0]);

    kill(child_pid, SIGTERM);
    waitpid(child_pid, NULL, 0);

    unlink("/tmp/task2.pid");

    syslog(LOG_INFO, "Daemon stopped cleanly");
    closelog();

    return 0;
}
