#include <iostream>
#include <cstdint>

class SearchEngine {
public:
    static std::int64_t InterpolationSearch(std::int64_t target, std::int64_t n) {
        std::int64_t low = 0;
        std::int64_t high = n - 1;
        std::int64_t steps = 0;

        while (low <= high) {
            ++steps;
            std::int64_t mid = low + (high - low) / 2;
            std::int64_t val = mid * 7;
            if (val == target) {
                return steps;
            }
            if (val < target) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
        return steps;
    }
};

int main() {
    std::int64_t n = 10000000;
    std::int64_t totalSteps = 0;
    for (std::int64_t q = 0; q < 5000000; ++q) {
        std::int64_t target = (q * 31) % (n * 7);
        totalSteps += SearchEngine::InterpolationSearch(target, n);
    }
    std::cout << "Binary Search Probe Steps: " << totalSteps << "\n";
    return 0;
}

