#include <stdio.h>
#include <stdint.h>

static double RunSpectralNorm(int64_t n) {
    double sum = 0.0;
    int64_t i = 0;
    while (i < n) {
        int64_t j = 0;
        double rowSum = 0.0;
        while (j < n) {
            int64_t ij = i + j;
            int64_t d = (ij * (ij + 1)) / 2 + i + 1;
            rowSum += 1.0 / (double)d;
            j += 1;
        }
        int64_t ii = i + i;
        int64_t dii = (ii * (ii + 1)) / 2 + i + 1;
        sum += rowSum / (double)dii;
        i += 1;
    }
    return sum;
}

int main(void) {
    int64_t n = 2500;
    double res = RunSpectralNorm(n);
    printf("C Spectral-Norm Result:\n%.9f\n", res);
    return 0;
}
