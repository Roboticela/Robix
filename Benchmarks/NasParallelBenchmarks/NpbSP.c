#include <stdio.h>
#include <stdint.h>

static double RunSP(int64_t numSteps) {
    double u0 = 1.0, u1 = 1.0, u2 = 1.0, u3 = 1.0, u4 = 1.0;
    double rhs0 = 2.0, rhs1 = 2.5, rhs2 = 3.0, rhs3 = 2.5, rhs4 = 2.0;

    double cDiag = 4.0;
    double off1 = -1.0;
    double off2 = 0.2;

    int64_t step = 0;
    while (step < numSteps) {
        double nu0 = (rhs0 - (off1 * u1 + off2 * u2)) / cDiag;
        double nu1 = (rhs1 - (off1 * (u0 + u2) + off2 * u3)) / cDiag;
        double nu2 = (rhs2 - (off1 * (u1 + u3) + off2 * (u0 + u4))) / cDiag;
        double nu3 = (rhs3 - (off1 * (u2 + u4) + off2 * u1)) / cDiag;
        double nu4 = (rhs4 - (off1 * u3 + off2 * u2)) / cDiag;

        u0 = nu0; u1 = nu1; u2 = nu2; u3 = nu3; u4 = nu4;

        rhs2 += 0.0001 * u2;

        step += 1;
    }

    return u0 + u1 + u2 + u3 + u4;
}

int main(void) {
    int64_t n = 5000000;
    double res = RunSP(n);
    printf("NPB SP Checksum:\n%.6f\n", res);
    return 0;
}

