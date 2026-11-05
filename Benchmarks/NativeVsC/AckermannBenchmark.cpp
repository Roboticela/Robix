#include <iostream>
#include <cstdint>

static std::int64_t Ackermann(std::int64_t m, std::int64_t n) {
    if (m == 0) return n + 1;
    if (n == 0) return Ackermann(m - 1, 1);
    return Ackermann(m - 1, Ackermann(m, n - 1));
}

int main() {
    std::int64_t res = Ackermann(3, 11);
    std::cout << "Ackermann(3, 11): " << res << "\n";
    return 0;
}

