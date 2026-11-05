package Benchmarks.Revcomp;

public Function RunRevcomp(Int n) -> Int {
    Int seed = 987654321;
    Int hash = 0;
    Int i = 0;
    while (i < n) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int b1 = (seed / 16) % 4;
        Int b2 = (seed / 64) % 4;
        Int b3 = (seed / 256) % 4;
        Int b4 = (seed / 1024) % 4;

        Int c1 = 3 - b1;
        Int c2 = 3 - b2;
        Int c3 = 3 - b3;
        Int c4 = 3 - b4;

        Int block1 = c4 * 35937 + c3 * 1089 + c2 * 33 + c1;
        hash = (hash * 1185921 + block1) % 1000000007;

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int b5 = (seed / 16) % 4;
        Int b6 = (seed / 64) % 4;
        Int b7 = (seed / 256) % 4;
        Int b8 = (seed / 1024) % 4;

        Int c5 = 3 - b5;
        Int c6 = 3 - b6;
        Int c7 = 3 - b7;
        Int c8 = 3 - b8;

        Int block2 = c8 * 35937 + c7 * 1089 + c6 * 33 + c5;
        hash = (hash * 1185921 + block2) % 1000000007;

        i = i + 8;
    }
    return hash;
}

public Function Main() -> Void {
    Int n = 10000000;
    Int res = RunRevcomp(n);
    Print("Robix Native Reverse-Complement Result:");
    Print(res);
}
