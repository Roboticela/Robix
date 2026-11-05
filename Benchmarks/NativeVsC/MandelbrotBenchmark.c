#include <stdio.h>
#include <stdint.h>

static int64_t ComputeMandelbrotPoint(double cr, double ci, int64_t maxIter) {
    double zr = 0.0;
    double zi = 0.0;
    int64_t iter = 0;
    while (iter < maxIter) {
        double zr2 = zr * zr;
        double zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            return iter;
        }
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
        iter += 1;
    }
    return maxIter;
}

int main(void) {
    int64_t width = 500;
    int64_t height = 500;
    int64_t totalEscaped = 0;

    double rMin = -2.0;
    double rMax = 1.0;
    double iMin = -1.5;
    double iMax = 1.5;

    double rStep = (rMax - rMin) / 500.0;
    double iStep = (iMax - iMin) / 500.0;

    int64_t y = 0;
    double ci = iMin;
    while (y < height) {
        int64_t x = 0;
        double cr = rMin;
        while (x < width) {
            int64_t iters = ComputeMandelbrotPoint(cr, ci, 100);
            if (iters < 100) {
                totalEscaped += 1;
            }
            cr += rStep;
            x += 1;
        }
        ci += iStep;
        y += 1;
    }

    printf("Mandelbrot Escaped: %lld\n", (long long)totalEscaped);
    return 0;
}

