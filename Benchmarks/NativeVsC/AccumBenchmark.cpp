#include <iostream>
#include <cstdint>

class AccumEngine {
public:
    static std::int64_t Accumulate(std::int64_t outer, std::int64_t inner) {
        std::int64_t total = 0;
        for (std::int64_t i = 0; i < outer; ++i) {
            std::int64_t sub = 0;
            for (std::int64_t j = 0; j < inner; ++j) {
                sub += (i * 3 + j * 5) % 97;
            }
            total += sub;
        }
        return total;
    }
};

int main() {
    std::int64_t res = AccumEngine::Accumulate(50000, 1000);
    std::cout << "Nested Accum Total: " << res << "\n";
    return 0;
}

