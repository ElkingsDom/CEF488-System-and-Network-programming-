#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>

#define PORT 8080
#define BUF_SIZE 8192
#define MAX_EVENTS 1024

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void send_404(int client_fd)
{
    char response[] =
        "HTTP/1.0 404 Not Found\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<h1>404 Not Found</h1>";

    send(client_fd, response, strlen(response), 0);
}

void handle_client(int client_fd)
{
    char buffer[BUF_SIZE];
    char method[16];
    char path[256];

    memset(buffer, 0, sizeof(buffer));

    int bytes = recv(client_fd, buffer, sizeof(buffer), 0);

    if (bytes <= 0)
        return;

    sscanf(buffer, "%s %s", method, path);

    if (strstr(path, ".."))
    {
        send_404(client_fd);
        return;
    }

    char filepath[512] = "../wwwroot";

    if (strcmp(path, "/") == 0)
        strcat(filepath, "/small.html");
    else
        strcat(filepath, path);

    int file_fd = open(filepath, O_RDONLY);

    if (file_fd < 0)
    {
        send_404(client_fd);
        return;
    }

    struct stat st;
    stat(filepath, &st);

    char header[256];

    sprintf(header,
            "HTTP/1.0 200 OK\r\n"
            "Content-Length: %ld\r\n"
            "Content-Type: text/html\r\n\r\n",
            st.st_size);

    send(client_fd, header, strlen(header), 0);

    while ((bytes = read(file_fd, buffer, sizeof(buffer))) > 0)
    {
        send(client_fd, buffer, bytes, 0);
    }

    close(file_fd);
}

int main()
{
    int server_fd;

    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;

    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    set_nonblocking(server_fd);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 128) < 0)
    {
        perror("listen");
        exit(1);
    }

    int epfd = epoll_create1(0);

    if (epfd < 0)
    {
        perror("epoll_create1");
        exit(1);
    }

    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_fd;

    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    printf("Epoll server running on port %d\n", PORT);

    while (1)
    {
        int n = epoll_wait(epfd,
                           events,
                           MAX_EVENTS,
                           -1);

        for (int i = 0; i < n; i++)
        {
            if (events[i].data.fd == server_fd)
            {
                while (1)
                {
                    int client_fd = accept(server_fd,
                                           NULL,
                                           NULL);

                    if (client_fd < 0)
                    {
                        if (errno == EAGAIN ||
                            errno == EWOULDBLOCK)
                            break;

                        break;
                    }

                    set_nonblocking(client_fd);

                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = client_fd;

                    epoll_ctl(epfd,
                              EPOLL_CTL_ADD,
                              client_fd,
                              &ev);
                }
            }
            else
            {
                int client_fd = events[i].data.fd;

                handle_client(client_fd);

                epoll_ctl(epfd,
                          EPOLL_CTL_DEL,
                          client_fd,
                          NULL);

                close(client_fd);
            }
        }
    }

    close(server_fd);

    return 0;
}
