package Benchmarks.DaCapo.Fop;

public Function RunFopLayout(Int numPages) -> Int {
    Int seed = 888777666;
    Int checksum = 0;

    Int pageHeight = 842;
    Int lineHeight = 14;
    Int fontAscent = 10;
    Int fontDescent = 4;

    Int i = 0;
    while (i < numPages) {
        Int lineCount = 0;
        Int line = 0;

        while (line < 60) {
            seed = (seed * 1103515245 + 12345) % 2147483648;
            Int wordLen1 = (seed / 65536) % 12 + 1;
            lineCount = lineCount + 1;
            checksum = (checksum * 31 + wordLen1 + lineCount) % 1000000007;

            seed = (seed * 1103515245 + 12345) % 2147483648;
            Int wordLen2 = (seed / 65536) % 12 + 1;
            lineCount = lineCount + 1;
            checksum = (checksum * 31 + wordLen2 + lineCount) % 1000000007;

            line = line + 2;
        }

        i = i + 1;
    }

    return checksum;
}

public Function Main() -> Void {
    Int n = 100000;
    Int res = RunFopLayout(n);
    Print("DaCapo FOP Checksum:");
    Print(res);
}
