#include <iostream>
#include <iomanip>
#include <cstdint>

static double RunNabSim(int64_t steps) {
    double r12 = 1.5, r23 = 1.4, r13 = 2.4;
    double v12 = 0.0, v23 = 0.0, v13 = 0.0;
    double kBond = 350.0;
    double r0 = 1.45;
    double dt = 0.0005;

    int64_t step = 0;
    while (step < steps) {
        double f12 = -kBond * (r12 - r0);
        double f23 = -kBond * (r23 - r0);
        double f13 = 14.4 / (r13 * r13);

        v12 += f12 * dt;
        v23 += f23 * dt;
        v13 += f13 * dt;

        r12 += v12 * dt;
        r23 += v23 * dt;
        r13 += v13 * dt;

        step += 1;
    }

    return r12 + r23 + r13 + v12 + v23 + v13;
}

int main() {
    int64_t n = 10000000;
    double res = RunNabSim(n);
    std::cout << "SPEC 644.nab_r Checksum:\n" << std::fixed << std::setprecision(6) << res << "\n";
    return 0;
}

