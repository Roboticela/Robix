package Benchmarks.CollatzFast;

public Function ComputeStepsFast(Int startVal) -> Int {
    Int n = startVal;
    Int steps = 0;
    while (n > 1) {
        if ((n & 1) == 0) {
            n = n >> 1;
        } else {
            n = 3 * n + 1;
        }
        steps = steps + 1;
    }
    return steps;
}

public Function Main() -> Void {
    Int limit = 2000000;
    Int maxSteps = 0;
    Int i = 1;
    while (i <= limit) {
        Int s = ComputeStepsFast(i);
        if (s > maxSteps) {
            maxSteps = s;
        }
        i = i + 1;
    }
    Print("Robix Native Fast Max Collatz Steps (1..2000000):");
    Print(maxSteps);
}

