package Benchmarks.CollatzMax;

public Function Main() -> Void {
    Int limit = 2000000;
    Int maxSteps = 0;
    Int i = 1;
    while (i <= limit) {
        Int n = i;
        Int steps = 0;
        while (n > 1) {
            if (n % 2 == 0) {
                n = n / 2;
            } else {
                n = 3 * n + 1;
            }
            steps = steps + 1;
        }
        if (steps > maxSteps) {
            maxSteps = steps;
        }
        i = i + 1;
    }
    Print("Robix Native Flattened Collatz Max Steps:");
    Print(maxSteps);
}

