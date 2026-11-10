package Robix.Memory;

public Function SeparatedBytes() -> Int {
    Unsafe {
        Pointer<Byte> buffer = Memory.Allocate(16);
        Byte first = 42;
        Byte second = 7;
        *buffer = first;
        Pointer<Byte> next = buffer + 8;
        *next = second;
        Byte readFirst = *buffer;
        Byte readSecond = *next;
        Memory.Free(buffer);
        return readFirst.ToInt() + readSecond.ToInt();
    }
    return 0;
}
