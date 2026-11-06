package Benchmarks.Micro.Arithmetic;

public Function Main() -> Void {
    Int sum = 0;
    for (Int i = 0; i < 20000000; i = i + 1) {
        sum = sum + (i % 1000);
    }
    Print(sum);
}
