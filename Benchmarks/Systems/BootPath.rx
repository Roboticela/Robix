package Benchmarks.Systems.BootPath;

public Function Main() -> Void {
    Int addr = 0;
    for (Int i = 0; i < 3000000; i = i + 1) {
        Int page = i / 512;
        Int slot = i % 512;
        addr = addr + page * 8 + slot;
    }
    Print(addr);
}
