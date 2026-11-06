package Examples.StageF;

import Pages;
import Descriptors;
import MemoryMap;
import SerialPort;

public Function Boot() -> Void {
    InstallPages();
    InstallGDT();
    if (HasMemory() == 1) {
        WriteByte(77);
    } else {
        WriteByte(88);
    }
}
