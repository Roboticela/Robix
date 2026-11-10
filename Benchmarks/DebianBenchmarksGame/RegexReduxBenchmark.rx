package Benchmarks.RegexRedux;

public Function RunRegexRedux(Int n) -> Int {
    Int seed = 246813579;
    Int result = 0;
    Int window = 0;
    Int i = 0;
    while (i < n) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int b = (seed / 65536) % 4;
        window = ((window * 4) + b) % 65536;

        if (b == 0) {
            if (window == 9204) { result = result + 31; }
            else if (window == 15420) { result = result + 41; }
            else if (window == 22100) { result = result + 47; }
            else if (window == 48112) { result = result + 59; }
        } else if (b == 3) {
            if (window == 1335) { result = result + 17; }
            else if (window == 34091) { result = result + 53; }
            else if (window == 61003) { result = result + 61; }
        } else if (b == 1) {
            if (window == 4821) { result = result + 23; }
        }

        i = i + 1;
    }
    return result;
}

public Function Main() -> Void {
    Int n = 10000000;
    Int res = RunRegexRedux(n);
    Print("Robix Native Regex-Redux Result:");
    Print(res);
}
