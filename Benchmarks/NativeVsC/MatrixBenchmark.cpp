#include <iostream>
#include <cstdint>

class MatrixEngine {
public:
    static std::int64_t Compute(std::int64_t n) {
        std::int64_t sum = 0;
        for (std::int64_t i = 0; i < n; ++i) {
            for (std::int64_t j = 0; j < n; ++j) {
                for (std::int64_t k = 0; k < n; ++k) {
                    std::int64_t aVal = (i * 37 + k) % 100;
                    std::int64_t bVal = (k * 19 + j) % 100;
                    sum += aVal * bVal;
                }
            }
        }
        return sum;
    }
};

int main() {
    std::int64_t size = 250;
    std::int64_t total = MatrixEngine::Compute(size);
    std::cout << "Matrix Multiply Sum: " << total << "\n";
    return 0;
}

