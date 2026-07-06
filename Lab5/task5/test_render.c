#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "mandelbrot.h"

/*
 * test_render - Local single-machine Mandelbrot render.
 *
 * Task 6: This program is the baseline for performance analysis.
 * Run it and note the elapsed time, then compare it with the
 * distributed render time reported by master.
 *
 *   speedup    = local_time / distributed_time
 *   efficiency = speedup / number_of_workers
 */
int main(void)
{
    const int    width    = 800;
    const int    height   = 600;
    const int    max_iter = 1000;
    const double center_x = -0.5;
    const double center_y =  0.0;
    const double zoom     =  0.005;

    unsigned char *image =
        malloc(width * height);

    if (!image)
    {
        perror("malloc");
        return 1;
    }

    printf("Rendering %dx%d Mandelbrot locally...\n",
           width, height);

    /* --- Start timer --------------------------------------------- */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (int row = 0; row < height; row++)
    {
        compute_row(&image[row * width],
                    row,
                    width,
                    height,
                    center_x,
                    center_y,
                    zoom,
                    max_iter);
    }

    /* --- Stop timer ---------------------------------------------- */
    clock_gettime(CLOCK_MONOTONIC, &t_end);

    double elapsed =
        (t_end.tv_sec  - t_start.tv_sec) +
        (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    write_ppm("local_mandelbrot.ppm",
              image,
              width,
              height);

    printf("\n=== Performance Baseline (Task 6) ===\n");
    printf("Local render time : %.3f seconds\n", elapsed);
    printf("Image saved to    : local_mandelbrot.ppm\n");
    printf("Use this time as T_single when computing:\n");
    printf("  speedup    = T_single / T_distributed\n");
    printf("  efficiency = speedup  / num_workers\n");
    printf("=====================================\n");

    free(image);

    return 0;
}
