package Benchmarks.DaCapo.Luindex;

public Function RunLuindex(Int numDocs) -> Int {
    Int seed = 192837465;
    Int checksum = 0;

    // Simulate inverted index posting lists for 8 vocabulary terms
    Int term0 = 0; Int term1 = 0; Int term2 = 0; Int term3 = 0;
    Int term4 = 0; Int term5 = 0; Int term6 = 0; Int term7 = 0;

    Int docId = 0;
    while (docId < numDocs) {
        // 8 words per document unrolled for maximum register allocation
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int t0 = (seed / 65536) % 8;
        if (t0 == 0) { term0 = (term0 * 17 + docId) % 1000000007; }
        else if (t0 == 1) { term1 = (term1 * 17 + docId) % 1000000007; }
        else if (t0 == 2) { term2 = (term2 * 17 + docId) % 1000000007; }
        else if (t0 == 3) { term3 = (term3 * 17 + docId) % 1000000007; }
        else if (t0 == 4) { term4 = (term4 * 17 + docId) % 1000000007; }
        else if (t0 == 5) { term5 = (term5 * 17 + docId) % 1000000007; }
        else if (t0 == 6) { term6 = (term6 * 17 + docId) % 1000000007; }
        else { term7 = (term7 * 17 + docId) % 1000000007; }

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int t1 = (seed / 65536) % 8;
        if (t1 == 0) { term0 = (term0 * 17 + docId) % 1000000007; }
        else if (t1 == 1) { term1 = (term1 * 17 + docId) % 1000000007; }
        else if (t1 == 2) { term2 = (term2 * 17 + docId) % 1000000007; }
        else if (t1 == 3) { term3 = (term3 * 17 + docId) % 1000000007; }
        else if (t1 == 4) { term4 = (term4 * 17 + docId) % 1000000007; }
        else if (t1 == 5) { term5 = (term5 * 17 + docId) % 1000000007; }
        else if (t1 == 6) { term6 = (term6 * 17 + docId) % 1000000007; }
        else { term7 = (term7 * 17 + docId) % 1000000007; }

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int t2 = (seed / 65536) % 8;
        if (t2 == 0) { term0 = (term0 * 17 + docId) % 1000000007; }
        else if (t2 == 1) { term1 = (term1 * 17 + docId) % 1000000007; }
        else if (t2 == 2) { term2 = (term2 * 17 + docId) % 1000000007; }
        else if (t2 == 3) { term3 = (term3 * 17 + docId) % 1000000007; }
        else if (t2 == 4) { term4 = (term4 * 17 + docId) % 1000000007; }
        else if (t2 == 5) { term5 = (term5 * 17 + docId) % 1000000007; }
        else if (t2 == 6) { term6 = (term6 * 17 + docId) % 1000000007; }
        else { term7 = (term7 * 17 + docId) % 1000000007; }

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int t3 = (seed / 65536) % 8;
        if (t3 == 0) { term0 = (term0 * 17 + docId) % 1000000007; }
        else if (t3 == 1) { term1 = (term1 * 17 + docId) % 1000000007; }
        else if (t3 == 2) { term2 = (term2 * 17 + docId) % 1000000007; }
        else if (t3 == 3) { term3 = (term3 * 17 + docId) % 1000000007; }
        else if (t3 == 4) { term4 = (term4 * 17 + docId) % 1000000007; }
        else if (t3 == 5) { term5 = (term5 * 17 + docId) % 1000000007; }
        else if (t3 == 6) { term6 = (term6 * 17 + docId) % 1000000007; }
        else { term7 = (term7 * 17 + docId) % 1000000007; }

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int t4 = (seed / 65536) % 8;
        if (t4 == 0) { term0 = (term0 * 17 + docId) % 1000000007; }
        else if (t4 == 1) { term1 = (term1 * 17 + docId) % 1000000007; }
        else if (t4 == 2) { term2 = (term2 * 17 + docId) % 1000000007; }
        else if (t4 == 3) { term3 = (term3 * 17 + docId) % 1000000007; }
        else if (t4 == 4) { term4 = (term4 * 17 + docId) % 1000000007; }
        else if (t4 == 5) { term5 = (term5 * 17 + docId) % 1000000007; }
        else if (t4 == 6) { term6 = (term6 * 17 + docId) % 1000000007; }
        else { term7 = (term7 * 17 + docId) % 1000000007; }

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int t5 = (seed / 65536) % 8;
        if (t5 == 0) { term0 = (term0 * 17 + docId) % 1000000007; }
        else if (t5 == 1) { term1 = (term1 * 17 + docId) % 1000000007; }
        else if (t5 == 2) { term2 = (term2 * 17 + docId) % 1000000007; }
        else if (t5 == 3) { term3 = (term3 * 17 + docId) % 1000000007; }
        else if (t5 == 4) { term4 = (term4 * 17 + docId) % 1000000007; }
        else if (t5 == 5) { term5 = (term5 * 17 + docId) % 1000000007; }
        else if (t5 == 6) { term6 = (term6 * 17 + docId) % 1000000007; }
        else { term7 = (term7 * 17 + docId) % 1000000007; }

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int t6 = (seed / 65536) % 8;
        if (t6 == 0) { term0 = (term0 * 17 + docId) % 1000000007; }
        else if (t6 == 1) { term1 = (term1 * 17 + docId) % 1000000007; }
        else if (t6 == 2) { term2 = (term2 * 17 + docId) % 1000000007; }
        else if (t6 == 3) { term3 = (term3 * 17 + docId) % 1000000007; }
        else if (t6 == 4) { term4 = (term4 * 17 + docId) % 1000000007; }
        else if (t6 == 5) { term5 = (term5 * 17 + docId) % 1000000007; }
        else if (t6 == 6) { term6 = (term6 * 17 + docId) % 1000000007; }
        else { term7 = (term7 * 17 + docId) % 1000000007; }

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int t7 = (seed / 65536) % 8;
        if (t7 == 0) { term0 = (term0 * 17 + docId) % 1000000007; }
        else if (t7 == 1) { term1 = (term1 * 17 + docId) % 1000000007; }
        else if (t7 == 2) { term2 = (term2 * 17 + docId) % 1000000007; }
        else if (t7 == 3) { term3 = (term3 * 17 + docId) % 1000000007; }
        else if (t7 == 4) { term4 = (term4 * 17 + docId) % 1000000007; }
        else if (t7 == 5) { term5 = (term5 * 17 + docId) % 1000000007; }
        else if (t7 == 6) { term6 = (term6 * 17 + docId) % 1000000007; }
        else { term7 = (term7 * 17 + docId) % 1000000007; }

        checksum = (checksum + term0 + term1 + term2 + term3 + term4 + term5 + term6 + term7) % 1000000007;
        docId = docId + 1;
    }

    return checksum;
}

public Function Main() -> Void {
    Int n = 500000;
    Int res = RunLuindex(n);
    Print("DaCapo Luindex Checksum:");
    Print(res);
}
