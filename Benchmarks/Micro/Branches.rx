package Benchmarks.Micro.Branches;

public Function Main() -> Void {
    Int acc = 0;
    for (Int i = 0; i < 15000000; i = i + 1) {
        Int m = i % 4;
        if (m == 0) {
            acc = acc + 1;
        } else if (m == 1) {
            acc = acc + 2;
        } else if (m == 2) {
            acc = acc + 3;
        } else {
            acc = acc + 4;
        }
    }
    Print(acc);
}
