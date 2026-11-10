package Robix.Threading;

// Native execution starts two operating-system threads. Each adds one
// value with an interlocked add, and the caller waits for both.
// The interpreter applies both additions on the calling thread and
// returns the same sum.

public Function Pair(Int left, Int right) -> Int {
    Int cell = 0;
    Unsafe {
        Pointer<Int> slot = &cell;
        return Thread.AddPair(slot, left, right);
    }
    return 0;
}
