package Benchmarks.Ackermann;

public Function Ackermann(Int m, Int n) -> Int {
    if (m == 0) {
        return n + 1;
    }
    if (n == 0) {
        return Ackermann(m - 1, 1);
    }
    return Ackermann(m - 1, Ackermann(m, n - 1));
}

public Function Main() -> Void {
    Int res = Ackermann(3, 11);
    Print("Robix Native Ackermann(3, 11):");
    Print(res);
}

