package Examples.Machine;

import SerialPort;

public Function InstallPages() -> Void {
    Unsafe {
        Pointer<Int> pml4 = Memory.Pages(2);
        Pointer<Int> pdpt = pml4 + 512;
        *pdpt = 131;
        *pml4 = Memory.Address(pdpt) | 3;
        Int root = Memory.Address(pml4);
        Cpu.WriteCR3(root);
        if ((Cpu.ReadCR3() & ~4095) == root) {
            WriteByte(80);
        } else {
            WriteByte(88);
        }
    }
}
