package Benchmarks.Primes;

public Function SieveCount(Int limit) -> Int {
    Int count = 1; // 2 is prime
    Int num = 3;
    while (num <= limit) {
        Int isPrime = 1;
        Int d = 3;
        while (d * d <= num) {
            if (num % d == 0) {
                isPrime = 0;
                break;
            }
            d = d + 2;
        }
        if (isPrime == 1) {
            count = count + 1;
        }
        num = num + 2;
    }
    return count;
}

public Function Main() -> Void {
    Int limit = 200000;
    Int total = SieveCount(limit);
    Print("Robix Native Fast Prime Sieve:");
    Print(total);
}
