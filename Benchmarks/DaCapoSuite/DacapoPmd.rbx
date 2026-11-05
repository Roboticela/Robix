package DaCapo.Pmd;

public Function RunPmdCheck(Int totalNodes) -> Int {
    Int violationsCount = 0;
    Int cyclomaticComplexity = 1;
    Int activeNesting = 0;
    Int seed = 88776655;

    Int i = 0;
    while (i < totalNodes) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int nodeType = (seed / 65536) % 8;
        Int tokenLen = 1 + ((seed / 1024) % 40);
        Int childCount = (seed / 256) % 4;

        if (nodeType < 4) {
            if (nodeType == 1) {
                activeNesting = 1;
            } else if (nodeType == 2) {
                activeNesting = 2;
            } else if (nodeType == 3) {
                activeNesting = activeNesting + 1;
                cyclomaticComplexity = cyclomaticComplexity + 1;
                if (childCount == 0) {
                    violationsCount = violationsCount + 1;
                }
            }
        } else {
            if (nodeType == 4) {
                activeNesting = activeNesting + 1;
                cyclomaticComplexity = cyclomaticComplexity + 1;
                if (activeNesting > 4) {
                    violationsCount = violationsCount + 1;
                }
            } else if (nodeType == 5) {
                if (tokenLen > 25) {
                    violationsCount = violationsCount + 1;
                }
            } else if (nodeType == 7) {
                if (activeNesting > 1) {
                    activeNesting = activeNesting - 1;
                }
            }
        }

        if (activeNesting > 6) {
            violationsCount = violationsCount + 1;
        }

        i = i + 1;
    }

    Int finalChecksum = (violationsCount * 31 + cyclomaticComplexity * 17 + activeNesting) % 1000000007;
    return finalChecksum;
}

public Function Main() -> Void {
    Int nodes = 10000000;
    Int res = RunPmdCheck(nodes);
    Print("DaCapo PMD Checksum:");
    Print(res);
}
