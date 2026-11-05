package Benchmarks.Fibonacci;

public Function ComputeFib(Int n) -> Int {
    Int a = 0;
    Int b = 1;
    Int i = 0;
    while (i < n) {
        Int temp = a + b;
        a = b;
        b = temp;
        i = i + 1;
    }
    return a;
}

public Function Main() -> Void {
    Int iterations = 100000000;
    Int sum = 0;
    Int k = 0;
    while (k < iterations) {
        sum = sum + (k % 2);
        k = k + 1;
    }
    Int fib45 = ComputeFib(45);
    Print("Robix Native Fibonacci(45):");
    Print(fib45);
    Print("Checksum:");
    Print(sum);
}
