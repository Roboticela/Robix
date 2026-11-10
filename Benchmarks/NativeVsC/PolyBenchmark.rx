package Benchmarks.Polynomial;

public Function HornerPoly(Int x, Int degree) -> Int {
    Int result = 1;
    Int i = 0;
    while (i < degree) {
        result = (result * x + (i * 17 + 3)) % 1000000007;
        i = i + 1;
    }
    return result;
}

public Function Main() -> Void {
    Int iterations = 1000000;
    Int sum = 0;
    Int x = 1;
    while (x <= iterations) {
        sum = (sum + HornerPoly(x % 100, 10)) % 1000000007;
        x = x + 1;
    }
    Print("Robix Native Horner Polynomial Evaluation Sum:");
    Print(sum);
}

