#include <iostream>
#include <cstdint>

class HailstoneWalker {
public:
    static std::int64_t ComputeHailstoneMax(std::int64_t startVal) {
        std::int64_t n = startVal;
        std::int64_t highest = n;
        while (n > 1) {
            if (n % 2 == 0) {
                n /= 2;
            } else {
                n = 3 * n + 1;
            }
            if (n > highest) {
                highest = n;
            }
        }
        return highest;
    }
};

int main() {
    std::int64_t limit = 2000000;
    std::int64_t maxPeak = 0;
    for (std::int64_t i = 1; i <= limit; ++i) {
        std::int64_t peak = HailstoneWalker::ComputeHailstoneMax(i);
        if (peak > maxPeak) {
            maxPeak = peak;
        }
    }
    std::cout << "Hailstone Peak: " << maxPeak << "\n";
    return 0;
}

