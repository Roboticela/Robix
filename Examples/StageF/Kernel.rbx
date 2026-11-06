package Examples.StageF;

import Pages;
import Descriptors;
import MemoryMap;
import Scheduler;
import Timer;

public Function Start() -> Void {
    Unsafe {
        Cpu.Cli();
    }
    InstallPages();
    InstallGDT();
    if (HasMemory() == 1) {
        WriteByte(77);
    } else {
        WriteByte(88);
    }
    RunOtherTask();
    WriteByte(66);
    ProveSwitch();
    ArmTimer();
}
