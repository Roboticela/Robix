#include <stdio.h>
#include <stdint.h>

static double ComputeDot(int64_t n) {
    double sum = 0.0;
    int64_t i = 0;
    while (i < n) {
        double a = (double)(i % 100);
        double b = (double)((i * 3 + 7) % 100);
        sum += a * b;
        i += 1;
    }
    return sum;
}

int main(void) {
    int64_t n = 50000000;
    double total = ComputeDot(n);
    printf("Vector Dot Sum: %.2f\n", total);
    return 0;
}

