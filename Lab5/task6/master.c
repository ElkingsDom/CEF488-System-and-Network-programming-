#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "protocol.h"
#include "mandelbrot.h"

#define MAX_WORKERS 3
#define CHUNK_SIZE 10

int main(int argc,char *argv[])
{
    if(argc!=2)
    {
        printf("Usage: %s <port>\n",argv[0]);
        return 1;
    }

    int width=800;
    int height=600;

    int port=atoi(argv[1]);

    int server=
        socket(AF_INET,
               SOCK_STREAM,
               0);

    struct sockaddr_in addr;

    memset(&addr,0,sizeof(addr));

    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_port=htons(port);

    bind(server,
         (struct sockaddr*)&addr,
         sizeof(addr));

    listen(server,MAX_WORKERS);

    printf("Waiting for workers...\n");

    int workers[MAX_WORKERS];

    for(int i=0;i<MAX_WORKERS;i++)
    {
        workers[i]=accept(server,NULL,NULL);

        printf("Worker %d connected\n",
               i+1);
    }

    unsigned char *image=
        malloc(width*height);

    uint32_t next_row=0;
    uint32_t completed=0;

    while(completed<height)
    {
        for(int i=0;i<MAX_WORKERS;i++)
        {
            if(recv_work_request(workers[i])<0)
                continue;

            if(next_row>=height)
            {
                send_terminate(workers[i]);
                continue;
            }

            struct task t;

            t.width=width;
            t.height=height;

            t.start_row=next_row;

            t.num_rows=
                (next_row+CHUNK_SIZE<=height)?
                CHUNK_SIZE:
                height-next_row;

            t.max_iter=1000;
            t.center_x=-0.5;
            t.center_y=0.0;
            t.zoom=0.005;

            send_task(workers[i],&t);

            next_row+=t.num_rows;

            struct result r;

            recv_result(workers[i],&r);

            memcpy(&image[r.start_row*width],
                   r.pixels,
                   r.num_rows*width);

            completed+=r.num_rows;

            printf("Completed rows %u-%u\n",
                   r.start_row,
                   r.start_row+r.num_rows-1);

            free(r.pixels);
        }
    }

    write_ppm("dynamic_mandelbrot.ppm",
              image,
              width,
              height);

    printf("\nDynamic rendering complete.\n");

    free(image);

    return 0;
}
