package Benchmarks.EulerTotient;

public Function Gcd(Int x, Int y) -> Int {
    Int a = x;
    Int b = y;
    while (b != 0) {
        Int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

public Function Phi(Int n) -> Int {
    Int count = 0;
    Int i = 1;
    while (i <= n) {
        if (Gcd(i, n) == 1) {
            count = count + 1;
        }
        i = i + 1;
    }
    return count;
}

public Function Main() -> Void {
    Int n = 3000;
    Int sumPhi = 0;
    Int k = 1;
    while (k <= n) {
        sumPhi = sumPhi + Phi(k);
        k = k + 1;
    }
    Print("Robix Native Sum of Totients (1..3000):");
    Print(sumPhi);
}
