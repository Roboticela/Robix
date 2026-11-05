#include <iostream>
#include <cstdint>

static int64_t RunImagickFilter(int64_t pixels) {
    int64_t seed = 314159265;
    int64_t checksum = 0;

    int64_t p0 = 128, p1 = 64, p2 = 200;
    int64_t p3 = 90,  p4 = 150, p5 = 30;
    int64_t p6 = 180, p7 = 210, p8 = 45;

    int64_t i = 0;
    while (i < pixels) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t newPixel = (seed / 65536) % 256;

        p0 = p1; p1 = p2; p2 = p3;
        p3 = p4; p4 = p5; p5 = p6;
        p6 = p7; p7 = p8; p8 = newPixel;

        int64_t sum = (p0 + p2 + p6 + p8) + 2 * (p1 + p3 + p5 + p7) + 4 * p4;
        int64_t filtered = sum / 16;

        checksum = (checksum * 33 + filtered) % 1000000007;
        i += 1;
    }

    return checksum;
}

int main() {
    int64_t n = 5000000;
    int64_t res = RunImagickFilter(n);
    std::cout << "SPEC 638.imagick_r Checksum:\n" << res << "\n";
    return 0;
}

