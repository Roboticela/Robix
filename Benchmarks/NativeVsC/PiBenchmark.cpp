#include <iostream>
#include <iomanip>
#include <cstdint>

int main() {
    std::int64_t n = 50000000;
    double sum = 0.0;
    double denom = 1.0;
    double sign = 1.0;
    std::int64_t i = 0;
    while (i < n) {
        sum += sign / denom;
        denom += 2.0;
        sign = 0.0 - sign;
        i += 1;
    }
    double piEstimate = sum * 4.0;
    std::cout << "Pi Leibniz: " << std::setprecision(6) << piEstimate << "\n";
    return 0;
}

