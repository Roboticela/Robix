package Benchmarks.Micro.Calls;

public Function Mix(Int x) -> Int {
    return x * 3 + 1;
}

public Function Main() -> Void {
    Int sum = 0;
    for (Int i = 0; i < 10000000; i = i + 1) {
        sum = sum + Mix(i);
    }
    Print(sum);
}
