package Tests.Conformance;

Test native "pointers" {
    Int speed = 100;
    Byte seven = 7;
    Byte bit = 0;
    Unsafe {
        Pointer<Int> sp = &speed;
        *sp = 250;
        Pointer<Byte> raw = Memory.Allocate(8);
        Pointer<Byte> mid = raw + 1;
        *mid = seven;
        bit = *mid;
        Memory.Free(raw);
    }
    Assert.Equal(speed, 250);
    Assert.Equal(bit.ToInt(), 7);
}
