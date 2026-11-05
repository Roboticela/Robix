#include <stdio.h>
#include <stdint.h>

static int64_t RunAvroraSim(int64_t numCycles) {
    int64_t r0 = 0, r1 = 1, r2 = 2, r3 = 3;
    int64_t pc = 0;
    int64_t flagZ = 0;

    int64_t cycle = 0;
    while (cycle < numCycles) {
        int64_t op = pc % 4;
        if (op < 2) {
            if (op == 0) {
                r0 = (r0 + r1) % 256;
                flagZ = (r0 == 0) ? 1 : 0;
            } else {
                r2 = (r2 - r3 + 256) % 256;
                flagZ = (r2 == 0) ? 1 : 0;
            }
        } else {
            if (op == 2) {
                r1 = (r1 + 1) % 256;
            } else {
                if (flagZ == 0) {
                    pc += 1;
                }
            }
        }

        pc = (pc + 1) % 1024;
        cycle += 1;
    }

    return r0 + r1 * 2 + r2 * 3 + r3 * 4 + pc;
}

int main(void) {
    int64_t n = 10000000;
    int64_t res = RunAvroraSim(n);
    printf("DaCapo Avrora Checksum:\n%lld\n", (long long)res);
    return 0;
}
