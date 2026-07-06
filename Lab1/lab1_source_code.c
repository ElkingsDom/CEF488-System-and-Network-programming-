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
#include <sys/file.h>
#include <sys/resource.h>
#include <time.h>

// Global flag for signal handling
volatile sig_atomic_t running = 1;

// ================= SIGNAL HANDLERS =================
void handle_term(int sig) {
 (void)sig; 
    running = 0; // stop loop safely
}

void handle_sigchld(int sig) {
 (void)sig;
    // reap all children (avoid zombies)
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// ================= DAEMONIZATION =================
void daemonize() {
    pid_t pid = fork();

    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // parent exits

    if (setsid() < 0) exit(EXIT_FAILURE);

    if (chdir("/") < 0) exit(EXIT_FAILURE);

    umask(0);

    // Close all file descriptors
    for (int i = sysconf(_SC_OPEN_MAX); i >= 0; i--) {
        close(i);
    }

    // Redirect stdin, stdout, stderr to /dev/null
    int fd = open("/dev/null", O_RDWR);
    dup(fd);
    dup(fd);
}

// ================= MAIN =================
int main() {
    int pipe1[2]; // parent → child
    int pipe2[2]; // child → parent
    pid_t child_pid;

    // Open syslog
    openlog("lab1_daemon", LOG_PID | LOG_CONS, LOG_DAEMON);

    // ---------------- PID FILE + LOCK ----------------
    int pid_fd = open("/tmp/mydaemon.pid", O_RDWR | O_CREAT, 0640);
    if (pid_fd < 0) {
        syslog(LOG_ERR, "PID file open failed");
        exit(EXIT_FAILURE);
    }

    if (flock(pid_fd, LOCK_EX | LOCK_NB) < 0) {
        syslog(LOG_ERR, "Daemon already running");
        exit(EXIT_FAILURE);
    }

    // ---------------- DAEMONIZE ----------------
    daemonize();

    // Write PID
    dprintf(pid_fd, "%d\n", getpid());

    // ---------------- SIGNALS ----------------
    struct sigaction sa;

    sa.sa_handler = handle_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &sa, NULL);

    // ---------------- RESOURCE LIMIT ----------------
    struct rlimit rl;
    rl.rlim_cur = 2;
    rl.rlim_max = 2;
    setrlimit(RLIMIT_NPROC, &rl);

    // ---------------- CREATE PIPES ----------------
    if (pipe(pipe1) < 0 || pipe(pipe2) < 0) {
        syslog(LOG_ERR, "Pipe creation failed");
        unlink("/tmp/mydaemon.pid");
        exit(EXIT_FAILURE);
    }

    // ---------------- FORK CHILD ----------------
    child_pid = fork();

    if (child_pid < 0) {
        syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
        unlink("/tmp/mydaemon.pid");
        exit(EXIT_FAILURE);
    }

    // ================= CHILD PROCESS =================
    if (child_pid == 0) {
        close(pipe1[1]); // close write end
        close(pipe2[0]); // close read end

        // Non-blocking read
        fcntl(pipe1[0], F_SETFL, O_NONBLOCK);

        int sum = 0;
        int num;

        while (1) {
            int r = read(pipe1[0], &num, sizeof(num));

            if (r == -1 && errno == EAGAIN) {
                usleep(100000); // 0.1 sec
                continue;
            }

            if (r == 0) break; // EOF

            sum += num;

            write(pipe2[1], &sum, sizeof(sum));
        }

        close(pipe1[0]);
        close(pipe2[1]);
        exit(EXIT_SUCCESS);
    }

    // ================= PARENT (DAEMON) =================
    close(pipe1[0]);
    close(pipe2[1]);

    // Non-blocking mode
    fcntl(pipe1[1], F_SETFL, O_NONBLOCK);
    fcntl(pipe2[0], F_SETFL, O_NONBLOCK);

    int num = 1;
    time_t last_response = time(NULL);

    while (running) {
        // Send data
        if (write(pipe1[1], &num, sizeof(num)) > 0) {
            num++;
        }

        // Try reading response
        int sum;
        int r = read(pipe2[0], &sum, sizeof(sum));

        if (r > 0) {
            syslog(LOG_INFO, "Received sum: %d", sum);
            last_response = time(NULL);
        } else if (r == -1 && errno != EAGAIN) {
            syslog(LOG_ERR, "Read error: %s", strerror(errno));
        }

        // Timeout check (5 seconds)
        if (time(NULL) - last_response > 5) {
            syslog(LOG_ERR, "Child timeout, terminating...");
            kill(child_pid, SIGTERM);
            break;
        }

        sleep(1);
    }

    // ---------------- CLEANUP ----------------
    kill(child_pid, SIGTERM);
    waitpid(child_pid, NULL, 0);

    close(pipe1[1]);
    close(pipe2[0]);

    unlink("/tmp/mydaemon.pid");

    syslog(LOG_INFO, "Daemon exiting cleanly");
    closelog();

    return 0;
}
