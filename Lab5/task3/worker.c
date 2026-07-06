#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr,
                "Usage: %s <server_ip> <port>\n",
                argv[0]);

        return 1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);

    int sock =
        socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;

    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family = AF_INET;

    server_addr.sin_port =
        htons(port);

    if (inet_pton(AF_INET,
                  server_ip,
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");

        close(sock);
        return 1;
    }

    printf("Connecting to master %s:%d...\n",
           server_ip,
           port);

    if (connect(sock,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");

        close(sock);
        return 1;
    }

    printf("Connected to master successfully.\n");

    printf("Waiting for tasks...\n");

    pause();

    close(sock);

    return 0;
}
