#include <stdio.h>
#include <stdint.h>

static double Integrate(int64_t samples) {
    double sum = 0.0;
    int64_t seed = 123456789;
    int64_t i = 0;
    while (i < samples) {
        seed = (seed * 1103515245 + 12345) % 1000000;
        double x = (double)seed / 1000000.0;
        double y = 4.0 / (1.0 + x * x);
        sum += y;
        i += 1;
    }
    return sum / (double)samples;
}

int main(void) {
    int64_t samples = 20000000;
    double approx = Integrate(samples);
    printf("Monte Carlo Integral: %.6f\n", approx);
    return 0;
}
