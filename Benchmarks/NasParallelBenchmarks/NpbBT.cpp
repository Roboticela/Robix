#include <iostream>
#include <iomanip>
#include <cstdint>

static double RunBT(int64_t numSteps) {
    double u0 = 1.0, u1 = 1.0, u2 = 1.0;
    double r0 = 2.0, r1 = 3.0, r2 = 1.5;

    double a1 = -0.5, a2 = -0.5;
    double b0 = 2.0,  b1 = 2.0,  b2 = 2.0;
    double c0 = -0.5, c1 = -0.5;

    int64_t step = 0;
    while (step < numSteps) {
        double invB0 = 1.0 / b0;
        double cPrime0 = c0 * invB0;
        double dPrime0 = r0 * invB0;

        double denom1 = b1 - a1 * cPrime0;
        double invDenom1 = 1.0 / denom1;
        double cPrime1 = c1 * invDenom1;
        double dPrime1 = (r1 - a1 * dPrime0) * invDenom1;

        double denom2 = b2 - a2 * cPrime1;
        double dPrime2 = (r2 - a2 * dPrime1) / denom2;

        u2 = dPrime2;
        u1 = dPrime1 - cPrime1 * u2;
        u0 = dPrime0 - cPrime0 * u1;

        r0 += 0.001 * u0;
        r1 += 0.001 * u1;
        r2 += 0.001 * u2;

        step += 1;
    }

    return u0 + u1 + u2 + r0 + r1 + r2;
}

int main() {
    int64_t n = 5000000;
    double res = RunBT(n);
    std::cout << "NPB BT Checksum:\n" << std::fixed << std::setprecision(6) << res << "\n";
    return 0;
}

