#include <stdio.h>
#include <stdint.h>

static double ChebyshevT(int64_t n, double x) {
    if (n == 0) return 1.0;
    if (n == 1) return x;
    double t0 = 1.0;
    double t1 = x;
    int64_t i = 2;
    while (i <= n) {
        double tNext = 2.0 * x * t1 - t0;
        t0 = t1;
        t1 = tNext;
        i += 1;
    }
    return t1;
}

int main(void) {
    int64_t evaluations = 20000000;
    double sum = 0.0;
    int64_t k = 0;
    while (k < evaluations) {
        double x = 0.5;
        sum += ChebyshevT(10, x);
        k += 1;
    }
    printf("Chebyshev Sum: %.6f\n", sum);
    return 0;
}

