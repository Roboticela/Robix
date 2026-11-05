#include <iostream>
#include <cstdint>

class BitCounter {
public:
    static std::int64_t CountSetBits(std::int64_t n) {
        std::int64_t count = 0;
        while (n > 0) {
            count += (n % 2);
            n /= 2;
        }
        return count;
    }
};

int main() {
    std::int64_t limit = 20000000;
    std::int64_t totalBits = 0;
    for (std::int64_t i = 1; i <= limit; ++i) {
        totalBits += BitCounter::CountSetBits(i);
    }
    std::cout << "Bit Count Total: " << totalBits << "\n";
    return 0;
}

