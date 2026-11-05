#include <iostream>
#include <cstdint>

class BitEngine {
public:
    static std::int64_t Compute(std::int64_t n) {
        std::int64_t acc = 123456789;
        for (std::int64_t i = 0; i < n; ++i) {
            acc = acc * 6364136223846793005ULL + 1442695040888963407ULL;
            acc = (acc * 33) + (i % 256);
        }
        return acc;
    }
};

int main() {
    std::int64_t n = 100000000;
    std::int64_t res = BitEngine::Compute(n);
    std::cout << "BitOps LCG Result: " << res << "\n";
    return 0;
}

