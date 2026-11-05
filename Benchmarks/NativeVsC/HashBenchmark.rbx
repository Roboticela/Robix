package Benchmarks.BitwiseHash;

public Function MurmurStep(Int h, Int k) -> Int {
    k = k * 1540483477;
    k = (k ^ (k / 65536)) * 1540483477;
    h = (h * 1540483477) ^ k;
    return h;
}

public Function Main() -> Void {
    Int iterations = 50000000;
    Int hash = 2166136261;
    Int i = 0;
    while (i < iterations) {
        Int k = i * 2654435761;
        hash = (hash * 1540483477) ^ k;
        i = i + 1;
    }
    Print("Robix Native Bitwise Hash Result:");
    Print(hash);
}

