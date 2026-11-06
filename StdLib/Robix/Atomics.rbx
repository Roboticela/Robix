package Robix.Atomics;

public Function Bump(Int start, Int delta) -> Int {
    Int cell = start;
    Unsafe {
        Pointer<Int> slot = &cell;
        Int old = Atomic.FetchAdd(slot, delta);
        if (old != start) {
            return 0 - 1;
        }
    }
    return cell;
}
