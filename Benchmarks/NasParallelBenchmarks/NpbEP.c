#include <stdio.h>
#include <stdint.h>

static double RunEP(int64_t numPairs) {
    int64_t seed = 271828182;
    double sumX = 0.0;
    double sumY = 0.0;
    int64_t insideCount = 0;

    int64_t i = 0;
    while (i < numPairs) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        double x = 2.0 * (double)((seed / 65536) % 1000) / 1000.0 - 1.0;

        seed = (seed * 1103515245 + 12345) % 2147483648;
        double y = 2.0 * (double)((seed / 65536) % 1000) / 1000.0 - 1.0;

        double rSq = x * x + y * y;
        if (rSq <= 1.0) {
            sumX += x;
            sumY += y;
            insideCount += 1;
        }

        i += 1;
    }

    return sumX + sumY + (double)insideCount;
}

int main(void) {
    int64_t n = 5000000;
    double res = RunEP(n);
    printf("NPB EP Checksum:\n%.6f\n", res);
    return 0;
}

