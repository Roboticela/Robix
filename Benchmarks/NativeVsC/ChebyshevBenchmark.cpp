#include <iostream>
#include <iomanip>
#include <cstdint>

class OrthogonalPoly {
public:
    static double ChebyshevT(int64_t n, double x) {
        if (n == 0) return 1.0;
        if (n == 1) return x;
        double t0 = 1.0;
        double t1 = x;
        for (int64_t i = 2; i <= n; ++i) {
            double tNext = 2.0 * x * t1 - t0;
            t0 = t1;
            t1 = tNext;
        }
        return t1;
    }
};

int main() {
    int64_t evaluations = 20000000;
    double sum = 0.0;
    for (int64_t k = 0; k < evaluations; ++k) {
        double x = 0.5;
        sum += OrthogonalPoly::ChebyshevT(10, x);
    }
    std::cout << "Chebyshev Sum: " << std::fixed << std::setprecision(6) << sum << "\n";
    return 0;
}

