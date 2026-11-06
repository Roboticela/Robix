package Examples.StageF;

import SerialPort;

public Function TaskA() -> Void {
    WriteByte(65);
}

public Function TaskB() -> Void {
    WriteByte(66);
}

public Function RunTasks() -> Void {
    Unsafe {
        Pointer<Int> stackA = Memory.Pages(1);
        Pointer<Int> stackB = Memory.Pages(1);
        Cpu.Run(TaskA, Memory.Address(stackA + 512));
        Cpu.Run(TaskB, Memory.Address(stackB + 512));
        Cpu.Run(TaskA, Memory.Address(stackA + 512));
    }
}
