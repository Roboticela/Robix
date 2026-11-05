package Benchmarks.BinarySearch;

public Function InterpolationSearch(Int target, Int n) -> Int {
    Int low = 0;
    Int high = n - 1;
    Int steps = 0;

    while (low <= high) {
        steps = steps + 1;
        Int mid = low + (high - low) / 2;
        Int val = mid * 7;
        if (val == target) {
            return steps;
        }
        if (val < target) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return steps;
}

public Function Main() -> Void {
    Int n = 10000000;
    Int totalSteps = 0;
    Int q = 0;
    while (q < 5000000) {
        Int target = (q * 31) % (n * 7);
        totalSteps = totalSteps + InterpolationSearch(target, n);
        q = q + 1;
    }
    Print("Robix Native Binary Search Total Probe Steps:");
    Print(totalSteps);
}

