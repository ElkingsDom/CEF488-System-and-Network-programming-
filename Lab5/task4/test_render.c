#include <stdio.h>
#include <stdlib.h>

#include "mandelbrot.h"

int main()
{
    int width = 800;
    int height = 600;

    int max_iter = 1000;

    double center_x = -0.5;
    double center_y = 0.0;

    double zoom = 0.005;

    unsigned char *image =
        malloc(width * height);

    if (!image)
    {
        perror("malloc");
        return 1;
    }

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

        printf("Computed row %d/%d\n",
               row,
               height);
    }

    write_ppm("mandelbrot.ppm",
              image,
              width,
              height);

    printf("Image written to mandelbrot.ppm\n");

    free(image);

    return 0;
}
