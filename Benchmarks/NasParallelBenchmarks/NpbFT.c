#include <stdio.h>
#include <stdint.h>

static double RunFT(int64_t numSteps) {
    double r0 = 1.0, i0 = 0.0;
    double r1 = 0.5, i1 = 0.5;
    double r2 = 0.0, i2 = 1.0;
    double r3 = -0.5, i3 = 0.5;

    int64_t step = 0;
    while (step < numSteps) {
        double u0R = r0 + r2, u0I = i0 + i2;
        double u2R = r0 - r2, u2I = i0 - i2;

        double u1R = r1 + r3, u1I = i1 + i3;
        double diffR = r1 - r3, diffI = i1 - i3;
        double u3R = diffI;
        double u3I = -diffR;

        r0 = (u0R + u1R) * 0.25; i0 = (u0I + u1I) * 0.25;
        r1 = (u0R - u1R) * 0.25; i1 = (u0I - u1I) * 0.25;
        r2 = (u2R + u3R) * 0.25; i2 = (u2I + u3I) * 0.25;
        r3 = (u2R - u3R) * 0.25; i3 = (u2I - u3I) * 0.25;

        step += 1;
    }

    return r0 + i0 + r1 + i1 + r2 + i2 + r3 + i3;
}

int main(void) {
    int64_t n = 10000000;
    double res = RunFT(n);
    printf("NPB FT Checksum:\n%.6f\n", res);
    return 0;
}

