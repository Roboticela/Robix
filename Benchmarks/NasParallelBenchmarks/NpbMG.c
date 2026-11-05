#include <stdio.h>
#include <stdint.h>

static double RunMG(int64_t numCycles) {
    double u0 = 0.0, u1 = 0.0, u2 = 0.0, u3 = 0.0;
    double r0 = 1.0, r1 = 2.0, r2 = 1.5, r3 = 0.5;

    int64_t cycle = 0;
    while (cycle < numCycles) {
        u0 += 0.5 * r0;
        u1 += 0.5 * r1;
        u2 += 0.5 * r2;
        u3 += 0.5 * r3;

        r0 = 1.0 - (2.0 * u0 - u1);
        r1 = 2.0 - (2.0 * u1 - u0 - u2);
        r2 = 1.5 - (2.0 * u2 - u1 - u3);
        r3 = 0.5 - (2.0 * u3 - u2);

        double rc0 = 0.5 * r0 + 0.25 * r1;
        double rc1 = 0.25 * r2 + 0.5 * r3;

        double uc0 = 0.6 * rc0 + 0.2 * rc1;
        double uc1 = 0.2 * rc0 + 0.6 * rc1;

        u0 += uc0;
        u1 += 0.75 * uc0 + 0.25 * uc1;
        u2 += 0.25 * uc0 + 0.75 * uc1;
        u3 += uc1;

        u0 += 0.25 * r0;
        u1 += 0.25 * r1;
        u2 += 0.25 * r2;
        u3 += 0.25 * r3;

        cycle += 1;
    }

    return u0 + u1 + u2 + u3;
}

int main(void) {
    int64_t n = 5000000;
    double res = RunMG(n);
    printf("NPB MG Checksum:\n%.6f\n", res);
    return 0;
}

