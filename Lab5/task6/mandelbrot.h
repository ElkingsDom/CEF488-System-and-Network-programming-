#ifndef MANDELBROT_H
#define MANDELBROT_H

void compute_row(unsigned char *pixels,
                 int row,
                 int width,
                 int height,
                 double center_x,
                 double center_y,
                 double zoom,
                 int max_iter);

void write_ppm(const char *filename,
               const unsigned char *pixels,
               int width,
               int height);

#endif
