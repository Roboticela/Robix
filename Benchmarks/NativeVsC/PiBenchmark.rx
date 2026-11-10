package Benchmarks.PiLeibniz;

public Function Main() -> Void {
    Int n = 50000000;
    Float sum = 0.0;
    Float denom = 1.0;
    Float sign = 1.0;
    Int i = 0;
    while (i < n) {
        sum = sum + sign / denom;
        denom = denom + 2.0;
        sign = 0.0 - sign;
        i = i + 1;
    }
    Float piEstimate = sum * 4.0;
    Print("Robix Native Pi Leibniz:");
    Print(piEstimate);
}
