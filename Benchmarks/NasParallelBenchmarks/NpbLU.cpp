#include <iostream>
#include <iomanip>
#include <cstdint>

static double RunLU(int64_t numIters) {
    double x0 = 0.0, x1 = 0.0, x2 = 0.0, x3 = 0.0;
    double b0 = 1.0, b1 = 2.0, b2 = 3.0, b3 = 2.0;

    int64_t iter = 0;
    while (iter < numIters) {
        x0 = b0 / 3.0;
        x1 = (b1 - (-1.0 * x0)) / 3.0;
        x2 = (b2 - (-1.0 * x1)) / 3.0;
        x3 = (b3 - (-1.0 * x2)) / 3.0;

        x3 = (b3 - (-0.5 * x2)) / 3.0;
        x2 = (b2 - (-0.5 * x3 + -1.0 * x1)) / 3.0;
        x1 = (b1 - (-0.5 * x2 + -1.0 * x0)) / 3.0;
        x0 = (b0 - (-0.5 * x1)) / 3.0;

        b1 += 0.0001 * x1;
        b2 += 0.0001 * x2;

        iter += 1;
    }

    return x0 + x1 + x2 + x3 + b0 + b1 + b2 + b3;
}

int main() {
    int64_t n = 5000000;
    double res = RunLU(n);
    std::cout << "NPB LU Checksum:\n" << std::fixed << std::setprecision(6) << res << "\n";
    return 0;
}

