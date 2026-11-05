#include <stdio.h>
#include <stdint.h>

static int64_t RunAlphaBeta(int64_t depth, int64_t alpha, int64_t beta, int64_t posVal) {
    if (depth <= 0) {
        return posVal;
    }

    int64_t bestScore = -1000000;
    int64_t move = 0;
    while (move < 4) {
        int64_t delta = (move * 37 + depth * 13) % 29 - 14;
        int64_t childVal = -(posVal + delta);

        int64_t score = -RunAlphaBeta(depth - 1, -beta, -alpha, childVal);
        if (score > bestScore) {
            bestScore = score;
        }
        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            return bestScore;
        }

        move += 1;
    }
    return bestScore;
}

static int64_t RunDeepSJeng(int64_t numSearches) {
    int64_t checksum = 0;
    int64_t i = 0;
    while (i < numSearches) {
        int64_t rootPos = (i * 101 + 47) % 500 - 250;
        int64_t eval = RunAlphaBeta(11, -100000, 100000, rootPos);
        checksum = (checksum * 17 + eval) % 1000000007;
        i += 1;
    }
    return checksum;
}

int main(void) {
    int64_t n = 120;
    int64_t res = RunDeepSJeng(n);
    printf("SPEC 631.deepsjeng_r Checksum:\n%lld\n", (long long)res);
    return 0;
}

