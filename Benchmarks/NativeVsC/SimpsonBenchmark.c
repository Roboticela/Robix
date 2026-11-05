#include <stdio.h>
#include <stdint.h>

static double F(double x) {
    return 1.0 / (1.0 + x * x);
}

static double Integrate(double a, double b, int64_t n) {
    double h = (b - a) / (double)n;
    double sum = F(a) + F(b);

    int64_t i = 1;
    while (i < n) {
        double x = a + (double)i * h;
        if (i % 2 == 0) {
            sum += 2.0 * F(x);
        } else {
            sum += 4.0 * F(x);
        }
        i += 1;
    }
    return sum * (h / 3.0);
}

int main(void) {
    int64_t n = 20000000;
    double res = Integrate(0.0, 1.0, n);
    printf("Simpson 1/3 Result: %.6f\n", res);
    return 0;
}

