package Examples.StageF;

import SerialPort;

public Function OtherTask() -> Void {
    Unsafe {
        Int rsp = Cpu.ReadRSP();
        if (rsp >= 2097152) {
            WriteByte(83);
        } else {
            WriteByte(88);
        }
    }
}

public Function SwitchBack() -> Void {
    WriteByte(67);
    Unsafe {
        Cpu.ExchangeRSP(Cpu.SavedRSP());
    }
}

public Function TaskExit() -> Void {
    HaltCPU();
}

public Function RunOtherTask() -> Void {
    Unsafe {
        Pointer<Int> stack = Memory.Pages(1);
        Cpu.Run(OtherTask, Memory.Address(stack + 512));
    }
}

public Function ProveSwitch() -> Void {
    Unsafe {
        Pointer<Int> stack = Memory.Pages(1);
        *(stack + 510) = Cpu.Function(SwitchBack);
        *(stack + 511) = Cpu.Function(TaskExit);
        Cpu.ExchangeRSP(Memory.Address(stack + 510));
    }
}
