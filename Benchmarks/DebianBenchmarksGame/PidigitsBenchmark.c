#include <stdio.h>
#include <stdint.h>

static int64_t ModExp16(int64_t exp, int64_t mod) {
    int64_t res = 1;
    int64_t b = 16 % mod;
    int64_t e = exp;
    while (e > 0) {
        if (e % 2 == 1) res = (res * b) % mod;
        b = (b * b) % mod;
        e /= 2;
        if (e > 0) {
            if (e % 2 == 1) res = (res * b) % mod;
            b = (b * b) % mod;
            e /= 2;
        }
    }
    return res;
}

static int64_t ComputePiDigit(int64_t n) {
    double s1 = 0.0, s4 = 0.0, s5 = 0.0, s6 = 0.0;
    int64_t k = 0;
    while (k <= n) {
        int64_t exp = n - k;
        int64_t d1 = 8 * k + 1, d4 = 8 * k + 4, d5 = 8 * k + 5, d6 = 8 * k + 6;
        s1 += (double)ModExp16(exp, d1) / (double)d1;
        s4 += (double)ModExp16(exp, d4) / (double)d4;
        s5 += (double)ModExp16(exp, d5) / (double)d5;
        s6 += (double)ModExp16(exp, d6) / (double)d6;
        k += 1;
    }

    s1 -= (double)((int64_t)s1);
    s4 -= (double)((int64_t)s4);
    s5 -= (double)((int64_t)s5);
    s6 -= (double)((int64_t)s6);

    k = n + 1;
    double p16 = 1.0 / 16.0;
    while (k <= n + 10) {
        s1 += p16 / (double)(8 * k + 1);
        s4 += p16 / (double)(8 * k + 4);
        s5 += p16 / (double)(8 * k + 5);
        s6 += p16 / (double)(8 * k + 6);
        p16 /= 16.0;
        k += 1;
    }

    double x = 4.0 * s1 - 2.0 * s4 - s5 - s6;
    x -= (double)((int64_t)x);
    if (x < 0.0) x += 1.0;
    int64_t digit = (int64_t)(x * 16.0) % 16;
    return digit;
}

static int64_t RunPidigits(int64_t numDigits) {
    int64_t checksum = 0;
    int64_t i = 0;
    while (i < numDigits) {
        int64_t d = ComputePiDigit(i);
        checksum = (checksum * 13 + d) % 1000000007;
        i += 1;
    }
    return checksum;
}

int main(void) {
    int64_t n = 500;
    int64_t res = RunPidigits(n);
    printf("C Pidigits Result:\n%lld\n", (long long)res);
    return 0;
}
