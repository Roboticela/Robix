package Benchmarks.Hailstone;

public Function ComputeHailstoneMax(Int startVal) -> Int {
    Int n = startVal;
    Int highest = n;
    while (n > 1) {
        if (n % 2 == 0) {
            n = n / 2;
        } else {
            n = 3 * n + 1;
        }
        if (n > highest) {
            highest = n;
        }
    }
    return highest;
}

public Function Main() -> Void {
    Int limit = 2000000;
    Int maxPeak = 0;
    Int i = 1;
    while (i <= limit) {
        Int peak = ComputeHailstoneMax(i);
        if (peak > maxPeak) {
            maxPeak = peak;
        }
        i = i + 1;
    }
    Print("Robix Native Highest Hailstone Peak (1..2000000):");
    Print(maxPeak);
}

