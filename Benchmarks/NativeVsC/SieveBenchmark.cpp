#include <iostream>
#include <cstdint>

class FastSieve {
public:
    static std::int64_t SieveCount(std::int64_t limit) {
        std::int64_t count = 1;
        for (std::int64_t num = 3; num <= limit; num += 2) {
            bool isPrime = true;
            for (std::int64_t d = 3; d * d <= num; d += 2) {
                if (num % d == 0) {
                    isPrime = false;
                    break;
                }
            }
            if (isPrime) ++count;
        }
        return count;
    }
};

int main() {
    std::int64_t limit = 200000;
    std::int64_t total = FastSieve::SieveCount(limit);
    std::cout << "Fast Prime Sieve: " << total << "\n";
    return 0;
}
