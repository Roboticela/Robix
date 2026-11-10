package Benchmarks.Micro.Scan;

public Function Main() -> Void {
    Int count = 0;
    for (Int i = 0; i < 8000000; i = i + 1) {
        Int code = (i * 17 + 3) % 251;
        if (code == 7) {
            count = count + 1;
        }
    }
    Print(count);
}
