package Examples.StageF;

import SerialPort;

public Function ReadSectorByte() -> Int {
    Unsafe {
        Port.Write8(0x1F6, 0xE0);
        Port.Write8(0x1F2, 1);
        Port.Write8(0x1F3, 0);
        Port.Write8(0x1F4, 0);
        Port.Write8(0x1F5, 0);
        Port.Write8(0x1F7, 0x20);
        Int spins = 0;
        Int status = Port.Read8(0x1F7);
        while (spins < 1000000) {
            if ((status & 1) != 0) {
                return 0;
            }
            if ((status & 128) == 0 && (status & 8) != 0) {
                Int word = Port.Read16(0x1F0);
                return word & 255;
            }
            spins = spins + 1;
            status = Port.Read8(0x1F7);
        }
        return 0;
    }
}

public Function Start() -> Void {
    Int value = ReadSectorByte();
    if (value == 68) {
        WriteByte(68);
    } else {
        WriteByte(88);
    }
    HaltCPU();
}
