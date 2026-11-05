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
        Int currentY = 0;
        Int lineCount = 0;

        while (currentY + lineHeight <= pageHeight) {
            seed = (seed * 1103515245 + 12345) % 2147483648;
            Int wordLen = (seed / 65536) % 12 + 1;
            currentY = currentY + fontAscent + fontDescent;
            lineCount = lineCount + 1;
            checksum = checksum * 31 + wordLen + lineCount;
            if (checksum >= 1000000007) {
                checksum = checksum % 1000000007;
            }
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
