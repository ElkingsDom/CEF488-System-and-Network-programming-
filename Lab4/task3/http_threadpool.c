#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define PORT 8080
#define BUF_SIZE 8192
#define THREAD_COUNT 4
#define QUEUE_SIZE 256

int queue[QUEUE_SIZE];
int front = 0;
int rear = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

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

    recv(client_fd, buffer, sizeof(buffer), 0);

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

    int bytes;

    while ((bytes = read(file_fd, buffer, sizeof(buffer))) > 0)
    {
        send(client_fd, buffer, bytes, 0);
    }

    close(file_fd);
}

void enqueue(int client_fd)
{
    pthread_mutex_lock(&mutex);

    queue[rear] = client_fd;
    rear = (rear + 1) % QUEUE_SIZE;

    pthread_cond_signal(&cond);

    pthread_mutex_unlock(&mutex);
}

int dequeue()
{
    pthread_mutex_lock(&mutex);

    while (front == rear)
    {
        pthread_cond_wait(&cond, &mutex);
    }

    int client_fd = queue[front];

    front = (front + 1) % QUEUE_SIZE;

    pthread_mutex_unlock(&mutex);

    return client_fd;
}

void *worker_thread(void *arg)
{
    while (1)
    {
        int client_fd = dequeue();

        handle_client(client_fd);

        close(client_fd);
    }

    return NULL;
}

int main()
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t addrlen = sizeof(client_addr);

    pthread_t threads[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++)
    {
        pthread_create(&threads[i],
                       NULL,
                       worker_thread,
                       NULL);
    }

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

    printf("Thread pool server running on port %d\n", PORT);

    while (1)
    {
        client_fd = accept(server_fd,
                           (struct sockaddr *)&client_addr,
                           &addrlen);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        enqueue(client_fd);
    }

    close(server_fd);

    return 0;
}
