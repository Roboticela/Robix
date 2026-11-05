#include <iostream>
#include <iomanip>
#include <cstdint>

class VectorMath {
public:
    static double ComputeDot(int64_t n) {
        double sum = 0.0;
        for (int64_t i = 0; i < n; ++i) {
            double a = (double)(i % 100);
            double b = (double)((i * 3 + 7) % 100);
            sum += a * b;
        }
        return sum;
    }
};

int main() {
    int64_t n = 50000000;
    double total = VectorMath::ComputeDot(n);
    std::cout << "Vector Dot Sum: " << std::fixed << std::setprecision(2) << total << "\n";
    return 0;
}

