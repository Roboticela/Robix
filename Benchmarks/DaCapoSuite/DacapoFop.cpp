#include <iostream>
#include <cstdint>

static int64_t RunFopLayout(int64_t numPages) {
    int64_t seed = 888777666;
    int64_t checksum = 0;

    int64_t pageHeight = 842;
    int64_t lineHeight = 14;
    int64_t fontAscent = 10;
    int64_t fontDescent = 4;

    int64_t i = 0;
    while (i < numPages) {
        int64_t currentY = 0;
        int64_t lineCount = 0;

        while (currentY + lineHeight <= pageHeight) {
            seed = (seed * 1103515245 + 12345) % 2147483648;
            int64_t wordLen = (seed / 65536) % 12 + 1;
            currentY += fontAscent + fontDescent;
            lineCount += 1;
            checksum = (checksum * 31 + wordLen + lineCount) % 1000000007;
        }

        i += 1;
    }

    return checksum;
}

int main() {
    int64_t n = 100000;
    int64_t res = RunFopLayout(n);
    std::cout << "DaCapo FOP Checksum:\n" << res << "\n";
    return 0;
}
