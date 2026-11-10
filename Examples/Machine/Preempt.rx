package Examples.Machine;

public Function TaskA() -> Void {
    while (1 == 1) {
        Unsafe {
            System.Call(65);
            Cpu.Halt();
        }
    }
}

public Function TaskB() -> Void {
    while (1 == 1) {
        Unsafe {
            System.Call(66);
            Cpu.Halt();
        }
    }
}

public Function Start() -> Void {
    Unsafe {
        Timer.Preempt(TaskA, TaskB);
    }
}
