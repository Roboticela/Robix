package Benchmarks.NestedAccum;

public Function Accumulate(Int outer, Int inner) -> Int {
    Int total = 0;
    Int i = 0;
    while (i < outer) {
        Int j = 0;
        Int sub = 0;
        while (j < inner) {
            sub = sub + (i * 3 + j * 5) % 97;
            sub = sub + (i * 3 + (j + 1) * 5) % 97;
            sub = sub + (i * 3 + (j + 2) * 5) % 97;
            sub = sub + (i * 3 + (j + 3) * 5) % 97;
            j = j + 4;
        }
        total = total + sub;
        i = i + 1;
    }
    return total;
}

public Function Main() -> Void {
    Int res = Accumulate(50000, 1000);
    Print("Robix Native Nested Accumulation Total:");
    Print(res);
}

