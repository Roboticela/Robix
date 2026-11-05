#include <stdio.h>
#include <stdint.h>

static int64_t RunLuindex(int64_t numDocs) {
    int64_t seed = 192837465;
    int64_t checksum = 0;

    int64_t term0 = 0, term1 = 0, term2 = 0, term3 = 0;
    int64_t term4 = 0, term5 = 0, term6 = 0, term7 = 0;

    int64_t docId = 0;
    while (docId < numDocs) {
        #define UPDATE_TERM() do { \
            seed = (seed * 1103515245 + 12345) % 2147483648; \
            int64_t term = (seed / 65536) % 8; \
            if (term == 0) { term0 = (term0 * 17 + docId) % 1000000007; } \
            else if (term == 1) { term1 = (term1 * 17 + docId) % 1000000007; } \
            else if (term == 2) { term2 = (term2 * 17 + docId) % 1000000007; } \
            else if (term == 3) { term3 = (term3 * 17 + docId) % 1000000007; } \
            else if (term == 4) { term4 = (term4 * 17 + docId) % 1000000007; } \
            else if (term == 5) { term5 = (term5 * 17 + docId) % 1000000007; } \
            else if (term == 6) { term6 = (term6 * 17 + docId) % 1000000007; } \
            else { term7 = (term7 * 17 + docId) % 1000000007; } \
        } while (0)

        UPDATE_TERM();
        UPDATE_TERM();
        UPDATE_TERM();
        UPDATE_TERM();
        UPDATE_TERM();
        UPDATE_TERM();
        UPDATE_TERM();
        UPDATE_TERM();

        checksum = (checksum + term0 + term1 + term2 + term3 + term4 + term5 + term6 + term7) % 1000000007;
        docId += 1;
    }

    return checksum;
}

int main(void) {
    int64_t n = 500000;
    int64_t res = RunLuindex(n);
    printf("DaCapo Luindex Checksum:\n%lld\n", (long long)res);
    return 0;
}
