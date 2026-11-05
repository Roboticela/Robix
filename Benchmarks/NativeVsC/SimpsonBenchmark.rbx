package Benchmarks.Simpson;

public Function F(Float x) -> Float {
    return 1.0 / (1.0 + x * x);
}

public Function Integrate(Float a, Float b, Int n) -> Float {
    Float h = (b - a) / n.ToFloat();
    Float sum = F(a) + F(b);

    Int i = 1;
    while (i < n) {
        Float x = a + i.ToFloat() * h;
        if (i % 2 == 0) {
            sum = sum + 2.0 * F(x);
        } else {
            sum = sum + 4.0 * F(x);
        }
        i = i + 1;
    }
    return sum * (h / 3.0);
}

public Function Main() -> Void {
    Int n = 20000000;
    Float res = Integrate(0.0, 1.0, n);
    Print("Robix Native Simpson 1/3 Integration Result:");
    Print(res);
}
