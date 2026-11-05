package Benchmarks.DaCapo.Xalan;

public Function RunXalanTransform(Int numNodes) -> Int {
    // Hierarchical XML DOM transform & attribute accumulation
    Int seed = 999888777;
    Int checksum = 0;

    Int depth = 0;
    Int tagCounter = 0;

    Int i = 0;
    while (i < numNodes) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int eventType = (seed / 65536) % 3; // 0: START_ELEM, 1: END_ELEM, 2: TEXT_NODE

        if (eventType == 0) {
            depth = depth + 1;
            tagCounter = tagCounter + 1;
            checksum = checksum * 33 + tagCounter + depth;
        } else if (eventType == 1) {
            if (depth > 0) {
                depth = depth - 1;
            }
            checksum = checksum * 31 + depth;
        } else {
            Int textLen = (seed / 16384) % 64 + 1;
            checksum = checksum * 37 + textLen * depth;
        }
        if (checksum >= 1000000007) {
            checksum = checksum % 1000000007;
        }

        i = i + 1;
    }

    return checksum;
}

public Function Main() -> Void {
    Int n = 5000000;
    Int res = RunXalanTransform(n);
    Print("DaCapo Xalan Checksum:");
    Print(res);
}

