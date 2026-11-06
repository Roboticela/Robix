package Robix.Collections;

public Function SumStored() -> Int {
    Array<Int> values = [10, 20, 0];
    values[2] = 12;
    Int total = 0;
    Int i = 0;
    while (i < values.Length()) {
        total = total + values[i];
        i = i + 1;
    }
    return total;
}
