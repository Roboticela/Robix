package Benchmarks.Micro.Allocate;

public Function Main() -> Void {
    Int sum = 0;
    Byte seven = 7;
    for (Int i = 0; i < 20000; i = i + 1) {
        Unsafe {
            Pointer<Byte> block = Memory.Allocate(64);
            *block = seven;
            Byte stored = *block;
            sum = sum + stored.ToInt();
            Memory.Free(block);
        }
    }
    Print(sum);
}
