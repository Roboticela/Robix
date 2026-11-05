#include <iostream>
#include <cstdint>

class PolynomialSolver {
public:
    static std::int64_t HornerPoly(std::int64_t x, std::int64_t degree) {
        std::int64_t result = 1;
        for (std::int64_t i = 0; i < degree; ++i) {
            result = (result * x + (i * 17 + 3)) % 1000000007;
        }
        return result;
    }
};

int main() {
    std::int64_t iterations = 1000000;
    std::int64_t sum = 0;
    for (std::int64_t x = 1; x <= iterations; ++x) {
        sum = (sum + PolynomialSolver::HornerPoly(x % 100, 10)) % 1000000007;
    }
    std::cout << "Poly Evaluation Sum: " << sum << "\n";
    return 0;
}

