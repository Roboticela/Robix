package Examples.StageF;

import SerialPort;

public Function GateLow(Int handler) -> Int {
    Int offsetLow = handler & 65535;
    Int offsetMid = (handler >> 16) & 65535;
    return offsetLow | (8 << 16) | (142 << 40) | (offsetMid << 48);
}

public Function GateHigh(Int handler) -> Int {
    return handler >> 32;
}

public Function SetGate(Pointer<Int> table, Int vector, Int handler) -> Void {
    Unsafe {
        Pointer<Int> gate = table + (vector * 2);
        *gate = GateLow(handler);
        *(gate + 1) = GateHigh(handler);
    }
}

public Function InstallGDT() -> Void {
    Unsafe {
        Pointer<Int> gdt = Memory.Pages(1);
        *gdt = 0;
        *(gdt + 1) = 0x00AF9A000000FFFF;
        *(gdt + 2) = 0x00CF92000000FFFF;
        Cpu.Lgdt(Memory.Address(gdt), 23);
        WriteByte(71);
    }
}
