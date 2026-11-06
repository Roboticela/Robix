package Examples.StageF;

import PageTable;
import SerialPort;

public Function Start() -> Void {
    Int entry = PageEntry(0x200000, 0x83);
    if (entry == 2097283) {
        WriteByte(82);
    } else {
        WriteByte(88);
    }
    HaltCPU();
}
