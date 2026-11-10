package Benchmarks.BitOps;

public Function ComputeBitPermutations(Int n) -> Int {
    Int acc = 123456789;
    Int i = 0;
    while (i < n) {
        acc = acc * 6364136223846793005 + 1442695040888963407;
        acc = (acc * 33) + (i % 256);
        i = i + 1;
    }
    return acc;
}

public Function Main() -> Void {
    Int n = 100000000;
    Int res = ComputeBitPermutations(n);
    Print("Robix Native BitOps LCG Result:");
    Print(res);
}

