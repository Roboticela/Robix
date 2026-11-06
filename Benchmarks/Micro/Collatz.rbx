package Benchmarks.Micro.Collatz;

public Function Steps(Int n) -> Int {
    Int current = n;
    Int steps = 0;
    while (current != 1) {
        if (current % 2 == 0) {
            current = current / 2;
        } else {
            current = 3 * current + 1;
        }
        steps = steps + 1;
        if (steps > 400) {
            return steps;
        }
    }
    return steps;
}

public Function Main() -> Void {
    Int sum = 0;
    for (Int n = 1; n <= 20000; n = n + 1) {
        sum = sum + Steps(n);
    }
    Print(sum);
}
