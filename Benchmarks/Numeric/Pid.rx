package Benchmarks.Numeric.Pid;

public Function Main() -> Void {
    Int integral = 0;
    Int prev = 0;
    Int acc = 0;
    for (Int i = 0; i < 2000000; i = i + 1) {
        Int err = 100 - (i % 200);
        integral = integral + err;
        if (integral > 100000) { integral = 100000; }
        if (integral < 0 - 100000) { integral = 0 - 100000; }
        Int deriv = err - prev;
        Int step = 3 * err + integral / 8 + 2 * deriv;
        if (step < 0) { step = 0 - step; }
        acc = (acc + step) % 1000003;
        prev = err;
    }
    Print(acc);
}
