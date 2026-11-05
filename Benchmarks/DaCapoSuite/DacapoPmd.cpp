#include <iostream>
#include <cstdint>

static int64_t RunPmdCheck(int64_t totalNodes) {
    int64_t violationsCount = 0;
    int64_t cyclomaticComplexity = 1;
    int64_t activeNesting = 0;
    int64_t seed = 88776655;

    int64_t i = 0;
    while (i < totalNodes) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t nodeType = (seed / 65536) % 8;
        int64_t tokenLen = 1 + ((seed / 1024) % 40);
        int64_t childCount = (seed / 256) % 4;

        if (nodeType < 4) {
            if (nodeType == 1) {
                activeNesting = 1;
            } else if (nodeType == 2) {
                activeNesting = 2;
            } else if (nodeType == 3) {
                activeNesting = activeNesting + 1;
                cyclomaticComplexity = cyclomaticComplexity + 1;
                if (childCount == 0) {
                    violationsCount = violationsCount + 1;
                }
            }
        } else {
            if (nodeType == 4) {
                activeNesting = activeNesting + 1;
                cyclomaticComplexity = cyclomaticComplexity + 1;
                if (activeNesting > 4) {
                    violationsCount = violationsCount + 1;
                }
            } else if (nodeType == 5) {
                if (tokenLen > 25) {
                    violationsCount = violationsCount + 1;
                }
            } else if (nodeType == 7) {
                if (activeNesting > 1) {
                    activeNesting = activeNesting - 1;
                }
            }
        }

        if (activeNesting > 6) {
            violationsCount = violationsCount + 1;
        }

        i = i + 1;
    }

    int64_t finalChecksum = (violationsCount * 31 + cyclomaticComplexity * 17 + activeNesting) % 1000000007;
    return finalChecksum;
}

int main() {
    int64_t nodes = 10000000;
    int64_t res = RunPmdCheck(nodes);
    std::cout << "DaCapo PMD Checksum:\n" << res << "\n";
    return 0;
}
