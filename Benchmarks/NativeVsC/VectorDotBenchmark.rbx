package Benchmarks.VectorDot;

public Function ComputeDot(Int n) -> Float {
    Float sum = 0.0;
    Int i = 0;
    while (i < n) {
        Float a = (Float)(i % 100);
        Float b = (Float)((i * 3 + 7) % 100);
        sum = sum + a * b;
        i = i + 1;
    }
    return sum;
}

public Function Main() -> Void {
    Int n = 50000000;
    Float total = ComputeDot(n);
    Print("Robix Native Synthetic Dot Product Sum:");
    Print(total);
}

