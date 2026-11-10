package Benchmarks.BinaryTrees;

public Function CheckTree(Int depth, Int item) -> Int {
    if (depth <= 0) {
        return item;
    }
    if (depth == 1) {
        return item - 1;
    }
    Int left = CheckTree(depth - 1, 2 * item - 1);
    Int right = CheckTree(depth - 1, 2 * item);
    return item + left - right;
}

public Function RunBinaryTrees(Int maxDepth) -> Int {
    Int checkSum = 0;
    
    // Stretch tree
    Int stretchDepth = maxDepth + 1;
    checkSum = checkSum + CheckTree(stretchDepth, 0);

    // Long-lived tree
    Int longLived = CheckTree(maxDepth, 0);

    // Iterative tree sweeps
    Int d = 4;
    while (d <= maxDepth) {
        Int iterations = 1;
        Int k = 0;
        Int power = maxDepth - d + 4;
        while (k < power) {
            iterations = iterations * 2;
            k = k + 1;
        }

        Int check = 0;
        Int i = 1;
        while (i <= iterations) {
            check = check + CheckTree(d, i);
            check = check + CheckTree(d, 0 - i);
            i = i + 1;
        }
        checkSum = checkSum + check;
        d = d + 2;
    }

    checkSum = checkSum + longLived;
    return checkSum;
}

public Function Main() -> Void {
    Int maxDepth = 16;
    Int res = RunBinaryTrees(maxDepth);
    Print("Robix Native Binary-Trees Result:");
    Print(res);
}
