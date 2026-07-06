#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "mandelbrot.h"

void compute_row(unsigned char *pixels,
                 int row,
                 int width,
                 int height,
                 double center_x,
                 double center_y,
                 double zoom,
                 int max_iter)
{
    for (int col = 0; col < width; col++)
    {
        double x0 = center_x +
                    (col - width / 2.0) * zoom;

        double y0 = center_y -
                    (row - height / 2.0) * zoom;

        double x = 0.0;
        double y = 0.0;

        int iter = 0;

        while ((x * x + y * y <= 4.0) &&
               (iter < max_iter))
        {
            double xtemp = x * x - y * y + x0;

            y = 2 * x * y + y0;
            x = xtemp;

            iter++;
        }

        pixels[col] =
            (unsigned char)(255.0 * iter / max_iter);
    }
}

void write_ppm(const char *filename,
               const unsigned char *pixels,
               int width,
               int height)
{
    FILE *fp = fopen(filename, "wb");

    if (!fp)
    {
        perror("fopen");
        return;
    }

    fprintf(fp, "P6\n%d %d\n255\n", width, height);

    for (int i = 0; i < width * height; i++)
    {
        unsigned char pixel = pixels[i];

        fwrite(&pixel, 1, 1, fp);
        fwrite(&pixel, 1, 1, fp);
        fwrite(&pixel, 1, 1, fp);
    }

    fclose(fp);
}
