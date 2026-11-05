#include <iostream>
#include <iomanip>
#include <cstdint>

static double RunLbmSim(int64_t steps) {
    double f0 = 0.25, f1 = 0.12, f2 = 0.08, f3 = 0.15;
    double omega = 1.45;

    int64_t step = 0;
    while (step < steps) {
        double rho = f0 + f1 + f2 + f3;
        double ux = (f1 - f3) / rho;
        double uy = (f2 - f0) / rho;
        double uSq = ux * ux + uy * uy;

        double feq0 = 0.25 * rho * (1.0 - 1.5 * uSq);
        double feq1 = 0.125 * rho * (1.0 + 3.0 * ux + 4.5 * ux * ux - 1.5 * uSq);
        double feq2 = 0.125 * rho * (1.0 + 3.0 * uy + 4.5 * uy * uy - 1.5 * uSq);
        double feq3 = 0.125 * rho * (1.0 - 3.0 * ux + 4.5 * ux * ux - 1.5 * uSq);

        f0 -= omega * (f0 - feq0);
        f1 -= omega * (f1 - feq1);
        f2 -= omega * (f2 - feq2);
        f3 -= omega * (f3 - feq3);

        double tmp = f1;
        f1 = f3;
        f3 = tmp;

        step += 1;
    }

    return f0 + f1 + f2 + f3;
}

int main() {
    int64_t n = 5000000;
    double res = RunLbmSim(n);
    std::cout << "SPEC 619.lbm_r Checksum:\n" << std::fixed << std::setprecision(6) << res << "\n";
    return 0;
}

