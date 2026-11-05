package Benchmarks.MonteCarloIntegral;

public Function Integrate(Int samples) -> Float {
    Float sum = 0.0;
    Int seed = 123456789;
    Int i = 0;
    while (i < samples) {
        seed = (seed * 1103515245 + 12345) % 1000000;
        Float x = seed.ToFloat() / 1000000.0;
        Float y = 4.0 / (1.0 + x * x);
        sum = sum + y;
        i = i + 1;
    }
    return sum / samples.ToFloat();
}

public Function Main() -> Void {
    Int samples = 20000000;
    Float approx = Integrate(samples);
    Print("Robix Native Monte Carlo Integral of 4/(1+x^2):");
    Print(approx);
}
