package Benchmarks.Collatz;

public Function ComputeSteps(Int startVal) -> Int {
    Int n = startVal;
    Int steps = 0;
    while (n > 1) {
        if (n % 2 == 0) {
            n = n / 2;
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
        Int s = ComputeSteps(i);
        if (s > maxSteps) {
            maxSteps = s;
        }
        i = i + 1;
    }
    Print("Robix Native Max Collatz Steps (1..2000000):");
    Print(maxSteps);
}

