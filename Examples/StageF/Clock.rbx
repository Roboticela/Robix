package Examples.StageF;

import SerialPort;

public Function ReadClock() -> Void {
    Unsafe {
        Port.Write8(112, 0);
        Int second = Port.Read8(113);
        Int tens = second >> 4;
        Int ones = second & 15;
        if (tens <= 5 && ones <= 9) {
            WriteByte(75);
        } else {
            WriteByte(88);
        }
    }
}
