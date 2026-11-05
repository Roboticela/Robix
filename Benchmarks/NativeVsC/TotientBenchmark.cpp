#include <iostream>
#include <cstdint>

class NumberTheory {
public:
    static std::int64_t Gcd(std::int64_t a, std::int64_t b) {
        while (b != 0) {
            std::int64_t t = b;
            b = a % b;
            a = t;
        }
        return a;
    }

    static std::int64_t Phi(std::int64_t n) {
        std::int64_t count = 0;
        for (std::int64_t i = 1; i <= n; ++i) {
            if (Gcd(i, n) == 1) {
                ++count;
            }
        }
        return count;
    }
};

int main() {
    std::int64_t n = 10000;
    std::int64_t sumPhi = 0;
    for (std::int64_t k = 1; k <= n; ++k) {
        sumPhi += NumberTheory::Phi(k);
    }
    std::cout << "Sum of Totients: " << sumPhi << "\n";
    return 0;
}

