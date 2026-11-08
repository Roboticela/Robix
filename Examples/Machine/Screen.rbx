package Examples.Machine;

import SerialPort;
import MemoryMap;

public Function Show() -> Void {
    Unsafe {
        Pointer<Byte> cell = Memory.At(753664);
        Byte mark = 86;
        Byte ink = 7;
        *cell = mark;
        *(cell + 1) = ink;
        Byte shown = *cell;
        if (shown.ToInt() == 86) {
            WriteByte(86);
        } else {
            WriteByte(88);
        }
    }
}
