package Robix.Math;

public Function Gcd(Int a, Int b) -> Int {
    Int x = a;
    Int y = b;
    if (x < 0) {
        x = 0 - x;
    }
    if (y < 0) {
        y = 0 - y;
    }
    while (y != 0) {
        Int remainder = x % y;
        x = y;
        y = remainder;
    }
    return x;
}

public Function IntegerSqrt(Int n) -> Int {
    if (n <= 0) {
        return 0;
    }
    Int low = 0;
    Int high = n;
    Int answer = 0;
    while (low <= high) {
        Int mid = (low + high) / 2;
        if (mid != 0 && mid > n / mid) {
            high = mid - 1;
        } else {
            answer = mid;
            low = mid + 1;
        }
    }
    return answer;
}
