package Benchmarks.VectorDot;

public Function ComputeDot(Int n) -> Float {
    Float sum = 0.0;
    Int i = 0;
    while (i < n) {
        Int modA = i % 100;
        Int modB = (i * 3 + 7) % 100;
        Float a = modA.ToFloat();
        Float b = modB.ToFloat();
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
