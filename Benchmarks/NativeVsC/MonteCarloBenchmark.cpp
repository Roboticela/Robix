#include <iostream>
#include <iomanip>
#include <cstdint>

class MonteCarloSimulator {
public:
    static double Integrate(int64_t samples) {
        double sum = 0.0;
        int64_t seed = 123456789;
        for (int64_t i = 0; i < samples; ++i) {
            seed = (seed * 1103515245 + 12345) % 1000000;
            double x = (double)seed / 1000000.0;
            double y = 4.0 / (1.0 + x * x);
            sum += y;
        }
        return sum / (double)samples;
    }
};

int main() {
    int64_t samples = 20000000;
    double approx = MonteCarloSimulator::Integrate(samples);
    std::cout << "Monte Carlo Integral: " << std::fixed << std::setprecision(6) << approx << "\n";
    return 0;
}
