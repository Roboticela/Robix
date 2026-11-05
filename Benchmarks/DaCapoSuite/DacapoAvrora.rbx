package Benchmarks.DaCapo.Avrora;

public Function RunAvroraSim(Int numCycles) -> Int {
    // 8-bit AVR micro-controller instruction pipeline simulation
    // Registers: r0, r1, r2, r3, PC, Flags
    Int r0 = 0; Int r1 = 1; Int r2 = 2; Int r3 = 3;
    Int pc = 0;
    Int flagZ = 0;

    Int cycle = 0;
    while (cycle < numCycles) {
        Int op = pc % 4;
        if (op < 2) {
            if (op == 0) {
                // ADD r0, r1
                r0 = (r0 + r1) % 256;
                if (r0 == 0) { flagZ = 1; } else { flagZ = 0; }
            } else {
                // SUB r2, r3
                r2 = (r2 - r3 + 256) % 256;
                if (r2 == 0) { flagZ = 1; } else { flagZ = 0; }
            }
        } else {
            if (op == 2) {
                // INC r1
                r1 = (r1 + 1) % 256;
            } else {
                // BRNE conditional branch
                if (flagZ == 0) {
                    pc = pc + 1;
                }
            }
        }

        pc = (pc + 1) % 1024;
        cycle = cycle + 1;
    }

    return r0 + r1 * 2 + r2 * 3 + r3 * 4 + pc;
}

public Function Main() -> Void {
    Int n = 10000000;
    Int res = RunAvroraSim(n);
    Print("DaCapo Avrora Checksum:");
    Print(res);
}
