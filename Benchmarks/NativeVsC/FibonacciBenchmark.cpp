#include <iostream>
#include <cstdint>

static std::int64_t ComputeFib(std::int64_t n) {
    std::int64_t a = 0;
    std::int64_t b = 1;
    std::int64_t i = 0;
    while (i < n) {
        std::int64_t temp = a + b;
        a = b;
        b = temp;
        i = i + 1;
    }
    return a;
}

int main() {
    std::int64_t iterations = 100000000;
    std::int64_t sum = 0;
    std::int64_t k = 0;
    while (k < iterations) {
        sum = sum + (k % 2);
        k = k + 1;
    }
    std::int64_t fib45 = ComputeFib(45);
    std::cout << "Fibonacci(45): " << fib45 << ", Checksum: " << sum << "\n";
    return 0;
}

