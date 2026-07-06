CC=gcc
CFLAGS=-Wall -Wextra -O2

all: test master worker

test_render: test_render.c mandelbrot.c
	$(CC) $(CFLAGS) -o test_render test_render.c mandelbrot.c -lm

master: master.c protocol.c mandelbrot.c
	$(CC) $(CFLAGS) -o master master.c protocol.c mandelbrot.c -lm

worker: worker.c protocol.c mandelbrot.c
	$(CC) $(CFLAGS) -o worker worker.c protocol.c mandelbrot.c -lm

clean:
	rm -f test_render master worker *.ppm
