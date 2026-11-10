package Benchmarks.BitCount;

public Function CountSetBits(Int val) -> Int {
    Int n = val;
    Int count = 0;
    while (n > 0) {
        count = count + (n % 2);
        n = n / 2;
    }
    return count;
}

public Function Main() -> Void {
    Int limit = 20000000;
    Int totalBits = 0;
    Int i = 1;
    while (i <= limit) {
        totalBits = totalBits + CountSetBits(i);
        i = i + 1;
    }
    Print("Robix Native Bit Count Total (1..20000000):");
    Print(totalBits);
}
