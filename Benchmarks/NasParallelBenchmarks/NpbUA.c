#include <stdio.h>
#include <stdint.h>

static double RunUA(int64_t numSteps) {
    double e0 = 1.0, e1 = 0.5, e2 = 0.25, e3 = 0.125;
    double f01 = 0.75, f12 = 0.375, f23 = 0.1875;

    int64_t step = 0;
    while (step < numSteps) {
        double flux0 = f01 - (e0 - e1);
        double flux1 = f12 - (e1 - e2);
        double flux2 = f23 - (e2 - e3);

        e0 += 0.1 * flux0;
        e1 += 0.1 * (flux1 - flux0);
        e2 += 0.1 * (flux2 - flux1);
        e3 -= 0.1 * flux2;

        f01 = 0.5 * (e0 + e1);
        f12 = 0.5 * (e1 + e2);
        f23 = 0.5 * (e2 + e3);

        step += 1;
    }

    return e0 + e1 + e2 + e3 + f01 + f12 + f23;
}

int main(void) {
    int64_t n = 10000000;
    double res = RunUA(n);
    printf("NPB UA Checksum:\n%.6f\n", res);
    return 0;
}

