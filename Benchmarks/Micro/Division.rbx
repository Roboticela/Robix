package Benchmarks.Micro.Division;

public Function Main() -> Void {
    Int sum = 0;
    for (Int i = 1; i <= 5000000; i = i + 1) {
        sum = sum + (i * 13) / 7;
    }
    Print(sum);
}
