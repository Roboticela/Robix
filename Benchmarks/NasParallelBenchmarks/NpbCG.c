#include <stdio.h>
#include <stdint.h>

static double RunCG(int64_t numIters) {
    double x0 = 1.0, x1 = 1.0, x2 = 1.0, x3 = 1.0;
    double r0 = 0.5, r1 = 0.5, r2 = 0.5, r3 = 0.5;
    double p0 = 0.5, p1 = 0.5, p2 = 0.5, p3 = 0.5;

    int64_t iter = 0;
    while (iter < numIters) {
        double q0 = 3.0 * p0 - p1;
        double q1 = 3.0 * p1 - p0 - p2;
        double q2 = 3.0 * p2 - p1 - p3;
        double q3 = 3.0 * p3 - p2;

        double ddot = p0 * q0 + p1 * q1 + p2 * q2 + p3 * q3;
        double rho = r0 * r0 + r1 * r1 + r2 * r2 + r3 * r3;
        double alpha = rho / (ddot + 0.0001);

        x0 += alpha * p0;
        x1 += alpha * p1;
        x2 += alpha * p2;
        x3 += alpha * p3;

        r0 -= alpha * q0;
        r1 -= alpha * q1;
        r2 -= alpha * q2;
        r3 -= alpha * q3;

        double rhoNext = r0 * r0 + r1 * r1 + r2 * r2 + r3 * r3;
        double beta = rhoNext / (rho + 0.0001);

        p0 = r0 + beta * p0;
        p1 = r1 + beta * p1;
        p2 = r2 + beta * p2;
        p3 = r3 + beta * p3;

        iter += 1;
    }

    return x0 + x1 + x2 + x3 + r0 + r1 + r2 + r3;
}

int main(void) {
    int64_t n = 5000000;
    double res = RunCG(n);
    printf("NPB CG Checksum:\n%.6f\n", res);
    return 0;
}

