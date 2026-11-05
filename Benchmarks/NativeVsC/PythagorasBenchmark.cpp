#include <iostream>
#include <cstdint>

class PythagorasSolver {
public:
    static std::int64_t Solve(std::int64_t limit) {
        std::int64_t sum = 0;
        for (std::int64_t a = 1; a < limit; ++a) {
            for (std::int64_t b = a + 1; b < limit; ++b) {
                std::int64_t a2b2 = a * a + b * b;
                std::int64_t c = 1;
                while (c * c < a2b2) {
                    ++c;
                }
                if (c * c == a2b2 && a + b + c <= limit) {
                    sum += a + b + c;
                }
            }
        }
        return sum;
    }
};

int main() {
    std::int64_t limit = 1500;
    std::int64_t total = PythagorasSolver::Solve(limit);
    std::cout << "Pythagorean Triples Sum: " << total << "\n";
    return 0;
}

