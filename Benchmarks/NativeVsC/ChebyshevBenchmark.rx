package Benchmarks.Chebyshev;

public Function ChebyshevT(Int n, Float x) -> Float {
    if (n == 0) {
        return 1.0;
    }
    if (n == 1) {
        return x;
    }
    Float t0 = 1.0;
    Float t1 = x;
    Int i = 2;
    while (i <= n) {
        Float tNext = 2.0 * x * t1 - t0;
        t0 = t1;
        t1 = tNext;
        i = i + 1;
    }
    return t1;
}

public Function Main() -> Void {
    Int evaluations = 20000000;
    Float sum = 0.0;
    Int k = 0;
    while (k < evaluations) {
        Float x = 0.5;
        sum = sum + ChebyshevT(10, x);
        k = k + 1;
    }
    Print("Robix Native Chebyshev T_10 Evaluation Sum:");
    Print(sum);
}

