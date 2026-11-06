# Robix implementation status

This file records what has actually been compiled and run. A row is **Implemented** only when `robixc` accepts it and a program using it has been compiled or executed. **Partial** means the frontend or a document describes it, and the native compiler or the interpreter does not yet carry it through. **Unsupported** means a native or IR build reports an error, or the tool does not exist.

Checked against `Compiler/CodeGen/NativeBackend.cpp`, `Compiler/IR/SSAIR.cpp`, and `Examples/StageB/LibcShape.rbx`. Compiler version string: `robixc version 1.0.0`.

## What `robixc -O3` compiles today

Implemented for top-level functions:

- Packages, imports that resolve inside the compilation, and `Main()` with no parameters.
- `Bool`, fixed-width integers (`Int8`, `Int16`, `Int32`, `Int64`, `UInt8` / `Byte`, `UInt16`, `UInt32`, `UInt64`), `Int`, `Float` / `Double`. Unsigned compares use unsigned predicates. A fitting integer literal may initialize a narrower integer.
- `let`, `var`, `const`, and explicit types.
- `if` / `else`, `while`, numeric `for` (including `..` and `..=`), `break`, `continue`, `return`.
- Integer add, sub, mul, div, rem, comparisons, `and` / `or` / `not`, and the bitwise operators `&`, `|`, `^`, `<<`, `>>`, and `~`. Hex literals such as `0xFFF` are accepted. Release integer math wraps. It is not `nsw`. A positive power-of-two divisor lowers to a shift or mask; a negative dividend uses the toward-zero adjustment instead of `sdiv`. Signed `>>` is an arithmetic shift. `<<` and `>>` lower to LLVM `shl` and `ashr`, the same instructions Clang emits for those C operators.
- Floating-point arithmetic marked `fast` in release builds.
- Direct calls, including overloads distinguished by argument count.
- `extern` functions with the C calling convention. `Examples/StageB/LibcShape.rbx` calls `puts`. Hosted Robix functions stay `fastcc`. A freestanding build exports the entry symbol, `Start` unless `--entry` says otherwise, with the C calling convention.
- `Print` of booleans, integers, floats, and strings.
- String concatenation. Two visible literals become one global; other concatenations call `rbx_strcat`. `==` and `!=` use `strcmp`.
- Arrays, indexing, and slices (`..` and `..=`), with a bounds trap. Structs, enums, `Tuple`, `Result` (`Success` / `Error`), and `Option` (`Some` / `None`).
- `Unsafe` pointers: `&`, `*`, pointer add and subtract, `Memory.Allocate`, `Memory.Free`.
- Equality-chain `if` lowered to an LLVM `switch` when the chain is long enough.
- Native pipeline: SSA IR, LLVM IR text, `opt -O3` with vertical SLP and horizontal SLP off, a late reassociate pass, then `clang -O3 -march=native`.

Stage B exit run, both engines, checksum `506`:

```text
robix run Examples/StageB/LibcShape.rbx
robixc -O3 Examples/StageB/LibcShape.rbx -o Build/StageB.exe
```

Both printed `ok`, `Hello`, and `506`.

Stage C exit run, both engines, `Examples/StageC/Machine.rbx`:

```text
robix run Examples/StageC/Machine.rbx
robixc -O3 Examples/StageC/Machine.rbx -o Build/StageC.exe
```

Both printed `150` and then `40`. That program has a `Motor` class with a private field, a constructor, methods, and a destructor; `Drive`, `Walker`, and `Runner` with `override`; a `Box<Int>` instantiation; and `Leaf`, which adds one and returns. The `40` is the destructor printing the motor speed after `Main` prints the total. `Examples/StageC/PrivateField.rbx` is rejected with `R2008` because `Main` reads the private field.

Class objects are a pointer. Word 0 is the vtable. Fields follow, in alphabetical order, one word each. A call whose `new` is visible in the same function is direct. `MakeRunner` returns a `Runner` as a `Walker`, and that call stays virtual: the unoptimized IR has `call fastcc i64 %t7(ptr %v28)`. The interpreter walks `Runner` then `Walker` for the same call. `Box<Int>` is lowered as its own functions (`Box$Int.Get`, `Box$Int.Set`), not as the open template. `Leaf` is `load`, `add`, `ret`. It does not call `malloc`. There is no collector. Class locals with a destructor are destroyed, then `free`d, at the end of the scope. Reference counting is not implemented.

One interface's methods occupy the front of the vtable, sorted by name. A class with two interfaces is not a supported slot layout. C still cannot call a Robix function.

Stage D guide, `Documentation/Guide.md`. Both engines:

| Command | Result |
| :--- | :--- |
| `Examples/StageD/Hello.rbx` | `Hello, Robix` then `42`. `Int Add(Int a, Int b)` has no arrow |
| `Examples/StageD/Point.rbx` | `7` |
| `Examples/StageD/Loop.rbx` | `10` |
| `Examples/StageD/Divide.rbx` | `0` then `5` |
| `StdLib/Hosted.rbx` | `45`, `hosted`, `4`, `10`, `9` |
| `Examples/StageD/Ask.rbx` with lines `Ada` and `41` | `Ada` then `42` |
| `robix new HelloApp` then `HelloApp/Main.rbx` | `Hello from Robix` |
| `Examples/StageD/Mismatch.rbx` | `R1001`, `Expected: Int`, `Received: String` |

`Math.Abs`, `Math.Min`, `Math.Max`, and `Math.Clamp` are integer intrinsics. `Length` on a `String` is `strlen`. `Length` on an `Array` is the length word. `StdLib/Hosted.rbx` is a real program: `Abs`, `Min`, `Max`, `Clamp`, and an `IntList` of four slots. `robix run` stores `File` in a map. The native `Hosted.exe` wrote `Build/StageDHosted.txt` containing `hosted`. `import PageTable;` loads `PageTable.rbx` from the same directory. `match` and `try` are not compiled.

Stage E, `Benchmarks/Micro/README.md`. `RunBenchmarks.ps1 -Suite All -Runs 5 -CoolSeconds 15` with `robixc -O3` and Clang 18.1.8 at `-O3 -march=native`. The starting language rotates each round. Checksums matched. Printed score: 1 win, 12 ties, 0 losses, 1 unconfirmed (MemCopy). Collatz is a tie at 18.18 ms against C at 17.34 ms. The Calls win did not repeat: the same binaries later ran in 9.53, 9.95, and 10.05 ms. `n % 2 == 0` is `(n & 1) == 0`, and division by 2 on that even branch is an arithmetic shift. Integer `Print` calls `printf`. The link drops runtime functions the program does not call.

Stage F. `robixc --target x86_64-unknown-none -O3 Examples/StageF/Serial.rbx -o Build/StageF/serial.elf` linked a multiboot ELF32. QEMU (`qemu-system-x86_64 -cpu qemu64 -kernel Build/StageF/serial.elf -serial file:... -display none -machine pc`) wrote one byte, 82 (`R`). That image is the QEMU sample. It is not a BIOS, and it was not run on hardware.

`Start` is exported with the C calling convention and no hosted `main`. The image does not link `RobixNativeRuntime.obj`. `PageEntry`, `SwitchChecksum`, and `OnTimer` are in that image. `Start` writes `R` when their checksum is 8503 and `X` otherwise. The same three functions in `Examples/StageF/Checksum.rbx`, compiled with hosted `robixc -O3`, printed `8503`. `Examples/StageF/Checksum.c` compiled with `clang -O3` printed `8503`. That match is a Windows run, not QEMU.

| Sample | Where it ran | Result |
| :--- | :--- | :--- |
| `Examples/StageF/Serial.rbx` | QEMU | Serial byte 82 |
| `Examples/StageF/Serial.rbxb` (`Output: Kernel`) | QEMU, same image path | Serial byte 82 |
| `Examples/StageF/Firmware.rbxb` (`Output: Firmware`) | QEMU | Serial byte 82. Same trampoline and linker script as Kernel |
| `Examples/StageF/Checksum.rbx` and `Checksum.c` | Windows | Both printed 8503 |
| `Examples/StageF/Volatile.rbx` | Windows | Printed 65. A local `Int` standing in for a device register. Not a board test |
| `Examples/StageF/AsmIO.rbx` | Windows | `ASM("movq $1, $0", out: y, in: x)` printed 20 |
| `Examples/StageF/Abort.rbx` | LLVM IR, then object files | `call void @llvm.trap()` and no `malloc`. Objects only for `aarch64-unknown-none` (AArch64 relocatable), `riscv64-unknown-none` (RISC-V relocatable, clang triple `riscv64-unknown-elf`), and `x86_64-unknown-linux-gnu` (x86-64 relocatable). No cross linker was run |
| `Examples/StageF/BadMnemonic.rbx` | Diagnostic | `--target aarch64-unknown-none` reports `N9010` for `outb` |

`ASM("...")` still works. `in:`, `out:`, and `clobber:` are optional. A `$` immediate is escaped for LLVM when the statement has no operands. `Abort()` and `Panic()` are `llvm.trap` and do not allocate. `Volatile.Load` and `Volatile.Store` are volatile `i64` memory operations and require `Unsafe`.

`Examples/StageF/Kernel.rbx` calls `Boot`, `Show`, `ReadClock`, `RunTasks`, and `ArmTimer`. `Boot` loads the page table, the GDT, and checks the Multiboot map. `Show` writes the VGA cell at `0xB8000` and reads it back. `ReadClock` reads the CMOS seconds register. `RunTasks` runs `TaskA` and `TaskB` on their own stacks, then `TaskA` again. `ArmTimer` is `Timer.Arm(OnTick)`, which installs the gate, programs the PIC, and waits. `robixc Examples/StageF/Kernel.rbxb -O3 -o Build/StageF/kernel.elf` linked that image. QEMU wrote the nine serial bytes `PGMVKABAT` (80, 71, 77, 86, 75, 65, 66, 65, 84). `Examples/StageF/GateHost.rbx` printed `9163329906409472` and `0` on the interpreter and on the hosted native build. `Port`, `Cpu`, `Timer`, `Memory.Pages`, `Memory.Address`, and `Memory.At` in the interpreter stop with a message that they run in the freestanding image. The CPU still enters 64-bit mode through the assembly trampoline linked in front of `Start`. This image is a QEMU kernel entry, not a run on hardware.

`Examples/StageF/Preempt.rbx` runs `TaskA` and `TaskB`. Each task calls `System.Call` (`int 0x80`, which writes the byte to the serial port) and then halts. The timer interrupt saves that stack and resumes the other task. QEMU wrote `ABAB` (65, 66, 65, 66). `Examples/StageF/Disk.rbx` issues an ATA read of sector 0 through `Port.Write8`, `Port.Read8`, and `Port.Read16`. With `Build/StageF/disk.img` on the IDE bus, QEMU wrote `D` (68), the first byte of that sector. `Examples/StageF/Bios.s` is a 16-bit reset ROM, linked to `Build/StageF/bios.bin` (65536 bytes). QEMU `-bios` wrote `I` (73) and halted. That ROM is assembly because the reset vector is 16-bit real mode. It does not load a kernel. Robix still enters 64-bit mode through the trampoline. There is no per-process address space and no filesystem.

`Examples/StageF/Board.rbx` is the same program on every board: `Console.Write(82)` and `Cpu.Halt()`. A Kernel project links a boot image. QEMU wrote byte 82 for `BoardX64.rbxb` (`qemu-system-x86_64 -cpu qemu64 -machine pc -kernel`), `BoardA64.rbxb` (`qemu-system-aarch64 -machine virt -cpu cortex-a57 -kernel`, UART `0x09000000`), `BoardArm.rbxb` (`qemu-system-arm -machine virt -cpu cortex-a15 -kernel`, UART `0x09000000`), and `BoardRv.rbxb` (`qemu-system-riscv64 -machine virt -bios none -kernel`, UART `0x10000000`). `Cpu.Halt` is `hlt` on x86-64 and `wfi` on the other three. A plain `--target aarch64-unknown-none`, `arm-unknown-none`, or `riscv64-unknown-none` still writes a relocatable object. `Console` stops in the interpreter with the freestanding message. This is not a PC BIOS on ARM, and it is not a boot of the other QEMU system binaries.

`Examples/StageF/Bits.rbx` and `Bits.c` both print `8195`, `4096`, `240`, `127`, and `-2` on the interpreter and on the native build. `PageEntry` is `(phys & ~0xFFF) | flags`. The QEMU image rebuilt from that formula still writes byte 82. A 50,000,000-iteration mix loop, `Examples/StageF/BitLoop.rbx` and `BitLoop.c`, printed the same checksum `1638366881728`. After one warmup the three Robix times were 60.9, 33.6, and 33.0 ms, and the three C times were 65.6, 36.3, and 39.5 ms. That is one loop on this machine, not a new suite score. The earlier micro-suite ties were not re-run.

## Partial

| Feature | What exists | What does not |
| :--- | :--- | :--- |
| Shared frontend | Lexer, parser, type check, SSA IR for the scalar subset | The interpreter and the native compiler are not proven equivalent by a conformance suite |
| Bytecode interpreter | `Interpreter/VM/BytecodeVM.hpp` and a bytecode module | Not the default proof of language behavior |
| Strings | Literals, concatenation, `==` / `!=`, and `Print` | `.ToString()` is still a native error. Ordering comparisons are not implemented |
| Ranges | Numeric range loops, array index, and slices | `for x in collection` is still an IR error |
| `match`, `try`, `throw` | Parsed | SSA IR rejects them |
| Lambdas and object literals | Parsed | SSA IR rejects them |
| C ABI | Robix calls an `extern` C function. The freestanding entry is an unmangled C-convention symbol | Hosted Robix functions are still `fastcc`. C cannot call an arbitrary Robix function |
| Build profiles | `-O0` debug and `-O2`/`-O3`/`--release` | `performance`, `realtime`, `embedded`, `safety`, and `size` are not separate compiler modes |
| Targets | Windows x64 hosted link. Kernel projects boot in QEMU for `x86_64-unknown-none`, `aarch64-unknown-none`, `arm-unknown-none`, and `riscv64-unknown-none`. A plain `--target` for AArch64, ARM, RISC-V, or Linux still writes a relocatable object | No Linux executable. No boot image for the other QEMU machines |
| Benchmarks | `Benchmarks/Micro/README.md`: 12 ties, 0 losses, MemCopy unconfirmed | No manifest sets `upstream: true`. Other suite names are refused |
| Specification | `Specification/LanguageSpecification.md` and grammar files | The documents describe a larger language than this table |

## Unsupported on the native backend

`robixc` reports `N9001` or `I4010` for these. They are not native-complete.

- Global constants.
- `defer` and `using`.
- Garbage collection. There is no collector in the native runtime. `Memory.Allocate` / `Memory.Free` are explicit `malloc` / `free`, not a collector.
- OS intrinsics other than `Memory.Allocate` and `Memory.Free`.
- PWM and other device drivers. `GPIO.Read` on Windows returns error code 1 and writes `GPIO.Read is not available on this target`. It does not return a pin level.
- Packages as a shippable registry. `.rbxproj` / `.rbxp` tooling is not the native compile path.
- RobixUI (`.rbxu`) and Robix Network. A `.rbxb` file is read only for `Output` (`Kernel` or `Firmware`), `Target`, `Entry`, `Source`, and `Profile`.
- LSP over the wire, a VS Code extension, and a debugger attached to the native binary.

## Commands that exist

| Command | Status |
| :--- | :--- |
| `robixc File.rbx -o App -O3` | Implemented |
| `robixc --target x86_64-unknown-none -O3 Examples/StageF/Serial.rbx` | Implemented. QEMU serial byte 82 |
| `robixc --target aarch64-unknown-none`, `arm-unknown-none`, `riscv64-unknown-none`, `x86_64-unknown-linux-gnu` | Object file only. Recorded for `Examples/StageF/Abort.rbx` on AArch64, RISC-V, and Linux. `arm-unknown-none` is accepted and is an object unless the project `Output` is `Kernel` or `Firmware` |
| `robixc Examples/StageF/BoardX64.rbxb`, `BoardA64.rbxb`, `BoardArm.rbxb`, `BoardRv.rbxb` | Implemented. Each QEMU boot wrote serial byte 82 |
| `robixc Examples/StageF/Serial.rbxb` and `Firmware.rbxb` | Implemented. Both boot in QEMU and write byte 82 |
| `robixc --version`, `robixc --help`, `robixc --emit-llvm`, `robixc --keep-ir` | Implemented |
| `robix run File.rbx` | Interpreter. Recorded on Stage B (`506`), Stage C (`150`, then `40`), and the Stage D guide samples |
| `robix new Name` | Writes `Name/Main.rbx`. Recorded as `HelloApp`, which prints `Hello from Robix` |
| `robix test Tests/Conformance` | Implemented. Interpreter and native both passed arithmetic, wrap, branches, calls, arrays, structs, and pointers. Bytecode version 99 was refused before execution. `NativeOnly.test.rbx` stopped on `Port.Write8` with the freestanding diagnostic. The native-only list is `Tests/Conformance/NATIVE_ONLY.md` |
| `robix test StdLib/Robix` | Implemented. `conformance: both engines passed` for Core, Collections, Math, String, IO, Memory, Atomics, Threading, Hardware, Robotics, and Encoding. `Build/StageH-io.txt` contains `RobixIO`. Windows `GPIO.Read(13)` printed the not-available message on both engines and the test saw error tag 0, value 1. Native `Thread.AddPair` starts two Windows threads. The interpreter applies those two additions on the calling thread and returns the same sum |
| `robix repl` | Not recorded as done |

## How to update this file

Change a row only after a command was run. Record the command and the result under the row. Do not mark a feature Implemented because a specification section or an example file mentions it.
