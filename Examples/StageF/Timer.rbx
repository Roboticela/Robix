package Examples.StageF;

import SerialPort;
import Descriptors;

public Function OnTick() -> Void {
    Unsafe {
        Port.Write8(0x20, 32);
    }
    WriteByte(84);
    HaltCPU();
}

public Function ArmTimer() -> Void {
    Unsafe {
        Pointer<Int> idt = Memory.Pages(1);
        SetGate(idt, 32, Cpu.Handler(OnTick));
        Cpu.Lidt(Memory.Address(idt), 4095);
        Port.Write8(0x20, 17);
        Port.Write8(0xA0, 17);
        Port.Write8(0x21, 32);
        Port.Write8(0xA1, 40);
        Port.Write8(0x21, 4);
        Port.Write8(0xA1, 2);
        Port.Write8(0x21, 1);
        Port.Write8(0xA1, 1);
        Port.Write8(0x21, 254);
        Port.Write8(0xA1, 255);
        Cpu.Sti();
        Cpu.Halt();
    }
}
