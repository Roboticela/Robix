#include <iostream>
#include <iomanip>
#include <cstdint>

class SimpsonIntegrator {
public:
    static inline double F(double x) {
        return 1.0 / (1.0 + x * x);
    }

    static double Integrate(double a, double b, int64_t n) {
        double h = (b - a) / (double)n;
        double sum = F(a) + F(b);

        for (int64_t i = 1; i < n; ++i) {
            double x = a + (double)i * h;
            sum += (i % 2 == 0 ? 2.0 : 4.0) * F(x);
        }
        return sum * (h / 3.0);
    }
};

int main() {
    int64_t n = 20000000;
    double res = SimpsonIntegrator::Integrate(0.0, 1.0, n);
    std::cout << "Simpson 1/3 Result: " << std::fixed << std::setprecision(6) << res << "\n";
    return 0;
}

