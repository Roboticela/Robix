package Benchmarks.Fasta;

public Function GenerateFasta(Int n) -> Int {
    Int seed = 42;
    Int countA = 0;
    Int countC = 0;
    Int countG = 0;
    Int countT = 0;
    Int hash = 0;

    Int i = 0;
    while (i < n) {
        seed = (seed * 3877 + 29573) % 139968;
        Int ch = 84;
        if (seed <= 37791) {
            ch = 65;
            countA = countA + 1;
        } else if (seed <= 79781) {
            ch = 67;
            countC = countC + 1;
        } else if (seed <= 121772) {
            ch = 71;
            countG = countG + 1;
        } else {
            countT = countT + 1;
        }
        hash = (hash * 31 + ch) % 1000000007;
        i = i + 1;
    }
    return hash + countA + countC + countG + countT;
}

public Function Main() -> Void {
    Int n = 5000000;
    Int res = GenerateFasta(n);
    Print("Robix Native Fasta Result:");
    Print(res);
}
