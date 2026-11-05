#include <stdio.h>
#include <stdint.h>

int main(void) {
    int64_t n = 50000000;
    double sum = 0.0;
    double denom = 1.0;
    double sign = 1.0;
    int64_t i = 0;
    while (i < n) {
        sum += sign / denom;
        denom += 2.0;
        sign = 0.0 - sign;
        i += 1;
    }
    double piEstimate = sum * 4.0;
    printf("C Native Pi Leibniz:\n");
    printf("%.5f\n", piEstimate);
    return 0;
}

