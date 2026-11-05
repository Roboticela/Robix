package Benchmarks.Euler;

public Function Solve(Int limit) -> Int {
    Int sum = 0;
    Int a = 1;
    while (a < limit) {
        Int b = a + 1;
        while (b < limit) {
            Int a2b2 = a * a + b * b;
            Int c = 1;
            while (c * c < a2b2) {
                c = c + 1;
            }
            if (c * c == a2b2) {
                if (a + b + c <= limit) {
                    sum = sum + a + b + c;
                }
            }
            b = b + 1;
        }
        a = a + 1;
    }
    return sum;
}

public Function Main() -> Void {
    Int limit = 1500;
    Int total = Solve(limit);
    Print("Robix Native Pythagorean Triples Sum:");
    Print(total);
}

