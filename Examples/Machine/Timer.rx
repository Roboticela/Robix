package Examples.Machine;

import SerialPort;

public Function OnTick() -> Void {
    WriteByte(84);
    HaltCPU();
}

public Function ArmTimer() -> Void {
    Unsafe {
        Timer.Arm(OnTick);
    }
}
