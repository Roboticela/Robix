# Robix implementation status

This file records what has actually been compiled and run. A row is **Implemented** only when `robixc` accepts it and a program using it has been compiled or executed. **Partial** means the frontend or a document describes it, and the native compiler or the interpreter does not yet carry it through. **Unsupported** means a native or IR build reports an error, or the tool does not exist.

The commands below name the paths that were executed. Those example folders were later renamed. The same sources now live under `Examples/`, and `Examples/README.md` lists the current path for each one.

Checked against `Compiler/CodeGen/NativeBackend.cpp`, `Compiler/IR/SSAIR.cpp`, and `Examples/StageB/LibcShape.rx`. Compiler version string: `robixc version 1.0.0`.

## What `robixc -O3` compiles today

Implemented for top-level functions:

- Packages, imports that resolve inside the compilation, and `Main()` with no parameters.
- `Bool`, fixed-width integers (`Int8`, `Int16`, `Int32`, `Int64`, `UInt8` / `Byte`, `UInt16`, `UInt32`, `UInt64`), `Int`, `Float` / `Double`. Unsigned compares use unsigned predicates. A fitting integer literal may initialize a narrower integer.
- `let`, `var`, `const`, and explicit types.
- `if` / `else`, `while`, numeric `for` (including `..` and `..=`), `break`, `continue`, `return`.
- Integer add, sub, mul, div, rem, comparisons, `and` / `or` / `not`, and the bitwise operators `&`, `|`, `^`, `<<`, `>>`, and `~`. Hex literals such as `0xFFF` are accepted. Release integer math wraps. It is not `nsw`. A positive power-of-two divisor lowers to a shift or mask; a negative dividend uses the toward-zero adjustment instead of `sdiv`. Signed `>>` is an arithmetic shift. `<<` and `>>` lower to LLVM `shl` and `ashr`, the same instructions Clang emits for those C operators.
- Floating-point arithmetic marked `fast` in release builds.
- Direct calls, including overloads distinguished by argument count.
- `extern` functions with the C calling convention. `Examples/StageB/LibcShape.rx` calls `puts`. An `extern` function that has a body is exported under its source name with the C calling convention. Other hosted Robix functions stay `fastcc`. A freestanding build exports the entry symbol, `Start` unless `--entry` says otherwise, with the C calling convention.
- `Print` of booleans, integers, floats, and strings.
- String concatenation. Two visible literals become one global; other concatenations call `rx_strcat`. `==` and `!=` use `strcmp`.
- Arrays, indexing, and slices (`..` and `..=`), with a bounds trap. Structs, enums, `Tuple`, `Result` (`Success` / `Error`), and `Option` (`Some` / `None`).
- `Unsafe` pointers: `&`, `*`, pointer add and subtract, `Memory.Allocate`, `Memory.Free`.
- Equality-chain `if` lowered to an LLVM `switch` when the chain is long enough.
- Native pipeline: SSA IR, LLVM IR text, `opt -O3` with vertical SLP and horizontal SLP off, a late reassociate pass, then `clang -O3 -march=native`.

Stage B exit run, both engines, checksum `506`:

```text
robix run Examples/StageB/LibcShape.rx
robixc -O3 Examples/StageB/LibcShape.rx -o Build/StageB.exe
```

Both printed `ok`, `Hello`, and `506`.

Stage C exit run, both engines, `Examples/StageC/Machine.rx`:

```text
robix run Examples/StageC/Machine.rx
robixc -O3 Examples/StageC/Machine.rx -o Build/StageC.exe
```

Both printed `150` and then `40`. That program has a `Motor` class with a private field, a constructor, methods, and a destructor; `Drive`, `Walker`, and `Runner` with `override`; a `Box<Int>` instantiation; and `Leaf`, which adds one and returns. The `40` is the destructor printing the motor speed after `Main` prints the total. `Examples/StageC/PrivateField.rx` is rejected with `R2008` because `Main` reads the private field.

Class objects are a pointer. Word 0 is the vtable. Fields follow, in alphabetical order, one word each. A call whose `new` is visible in the same function is direct. `MakeRunner` returns a `Runner` as a `Walker`, and that call stays virtual: the unoptimized IR has `call fastcc i64 %t7(ptr %v28)`. The interpreter walks `Runner` then `Walker` for the same call. `Box<Int>` is lowered as its own functions (`Box$Int.Get`, `Box$Int.Set`), not as the open template. `Leaf` is `load`, `add`, `ret`. It does not call `malloc`. There is no collector. Class locals with a destructor are destroyed, then `free`d, at the end of the scope. Reference counting is not implemented.

One interface's methods occupy the front of the vtable, sorted by name. `Examples/StageM/Features.rx` is the later run for one class that implements two interfaces.

Stage D guide, `Documentation/Guide.md`. Both engines:

| Command | Result |
| :--- | :--- |
| `Examples/StageD/Hello.rx` | `Hello, Robix` then `42`. `Int Add(Int a, Int b)` has no arrow |
| `Examples/StageD/Point.rx` | `7` |
| `Examples/StageD/Loop.rx` | `10` |
| `Examples/StageD/Divide.rx` | `0` then `5` |
| `StdLib/Hosted.rx` | `45`, `hosted`, `4`, `10`, `9` |
| `Examples/StageD/Ask.rx` with lines `Ada` and `41` | `Ada` then `42` |
| `robix new HelloApp` then `HelloApp/Main.rx` | `Hello from Robix` |
| `Examples/StageD/Mismatch.rx` | `R1001`, `Expected: Int`, `Received: String` |

`Math.Abs`, `Math.Min`, `Math.Max`, and `Math.Clamp` are integer intrinsics. `Length` on a `String` is `strlen`. `Length` on an `Array` is the length word. `StdLib/Hosted.rx` is a real program: `Abs`, `Min`, `Max`, `Clamp`, and an `IntList` of four slots. `robix run` stores `File` in a map. The native `Hosted.exe` wrote `Build/StageDHosted.txt` containing `hosted`. `import PageTable;` loads `PageTable.rx` from the same directory.

Stage E, `Benchmarks/Micro/README.md`. `RunBenchmarks.ps1 -Suite All -Runs 5 -CoolSeconds 15` with `robixc -O3` and Clang 18.1.8 at `-O3 -march=native`. The starting language rotates each round. Checksums matched. Printed score: 1 win, 12 ties, 0 losses, 1 unconfirmed (MemCopy). Collatz is a tie at 18.18 ms against C at 17.34 ms. The Calls win did not repeat: the same binaries later ran in 9.53, 9.95, and 10.05 ms. `n % 2 == 0` is `(n & 1) == 0`, and division by 2 on that even branch is an arithmetic shift. Integer `Print` calls `printf`. The link drops runtime functions the program does not call.

Stage F. `robixc --target x86_64-unknown-none -O3 Examples/StageF/Serial.rx -o Build/StageF/serial.elf` linked a multiboot ELF32. QEMU (`qemu-system-x86_64 -cpu qemu64 -kernel Build/StageF/serial.elf -serial file:... -display none -machine pc`) wrote one byte, 82 (`R`). That image is the QEMU sample. It is not a BIOS, and it was not run on hardware.

`Start` is exported with the C calling convention and no hosted `main`. The image does not link `RobixNativeRuntime.obj`. `PageEntry`, `SwitchChecksum`, and `OnTimer` are in that image. `Start` writes `R` when their checksum is 8503 and `X` otherwise. The same three functions in `Examples/StageF/Checksum.rx`, compiled with hosted `robixc -O3`, printed `8503`. `Examples/StageF/Checksum.c` compiled with `clang -O3` printed `8503`. That match is a Windows run, not QEMU.

| Sample | Where it ran | Result |
| :--- | :--- | :--- |
| `Examples/StageF/Serial.rx` | QEMU | Serial byte 82 |
| `Examples/StageF/Serial.rxb` (`Output: Kernel`) | QEMU, same image path | Serial byte 82 |
| `Examples/StageF/Firmware.rxb` (`Output: Firmware`) | QEMU | Serial byte 82. Same trampoline and linker script as Kernel |
| `Examples/StageF/Checksum.rx` and `Checksum.c` | Windows | Both printed 8503 |
| `Examples/StageF/Volatile.rx` | Windows | Printed 65. A local `Int` standing in for a device register. Not a board test |
| `Examples/StageF/AsmIO.rx` | Windows | `ASM("movq $1, $0", out: y, in: x)` printed 20 |
| `Examples/StageF/Abort.rx` | LLVM IR, then object files | `call void @llvm.trap()` and no `malloc`. Objects only for `aarch64-unknown-none` (AArch64 relocatable), `riscv64-unknown-none` (RISC-V relocatable, clang triple `riscv64-unknown-elf`), and `x86_64-unknown-linux-gnu` (x86-64 relocatable). No cross linker was run |
| `Examples/StageF/BadMnemonic.rx` | Diagnostic | `--target aarch64-unknown-none` reports `N9010` for `outb` |

`ASM("...")` still works. `in:`, `out:`, and `clobber:` are optional. A `$` immediate is escaped for LLVM when the statement has no operands. `Abort()` and `Panic()` are `llvm.trap` and do not allocate. `Volatile.Load` and `Volatile.Store` are volatile `i64` memory operations and require `Unsafe`.

`Examples/StageF/Kernel.rx` calls `Boot`, `Show`, `ReadClock`, `RunTasks`, and `ArmTimer`. `Boot` loads the page table, the GDT, and checks the Multiboot map. `Show` writes the VGA cell at `0xB8000` and reads it back. `ReadClock` reads the CMOS seconds register. `RunTasks` runs `TaskA` and `TaskB` on their own stacks, then `TaskA` again. `ArmTimer` is `Timer.Arm(OnTick)`, which installs the gate, programs the PIC, and waits. `robixc Examples/StageF/Kernel.rxb -O3 -o Build/StageF/kernel.elf` linked that image. QEMU wrote the nine serial bytes `PGMVKABAT` (80, 71, 77, 86, 75, 65, 66, 65, 84). `Examples/StageF/GateHost.rx` printed `9163329906409472` and `0` on the interpreter and on the hosted native build. `Port`, `CPU`, `Timer`, `Memory.Pages`, `Memory.Address`, and `Memory.At` in the interpreter stop with a message that they run in the freestanding image. The CPU still enters 64-bit mode through the assembly trampoline linked in front of `Start`. This image is a QEMU kernel entry, not a run on hardware.

`Examples/StageF/Preempt.rx` runs `TaskA` and `TaskB`. Each task calls `System.Call` (`int 0x80`, which writes the byte to the serial port) and then halts. The timer interrupt saves that stack and resumes the other task. QEMU wrote `ABAB` (65, 66, 65, 66). `Examples/StageF/Disk.rx` issues an ATA read of sector 0 through `Port.Write8`, `Port.Read8`, and `Port.Read16`. With `Build/StageF/disk.img` on the IDE bus, QEMU wrote `D` (68), the first byte of that sector. `Examples/StageF/Bios.s` is a 16-bit reset ROM, linked to `Build/StageF/bios.bin` (65536 bytes). QEMU `-bios` wrote `I` (73) and halted. That ROM is assembly because the reset vector is 16-bit real mode. It does not load a kernel. Robix still enters 64-bit mode through the trampoline. There is no per-process address space and no filesystem.

`Examples/StageF/Board.rx` is the same program on every board: `Console.Write(82)` and `CPU.Halt()`. A Kernel project links a boot image. QEMU wrote byte 82 for `BoardX64.rxb` (`qemu-system-x86_64 -cpu qemu64 -machine pc -kernel`), `BoardA64.rxb` (`qemu-system-aarch64 -machine virt -cpu cortex-a57 -kernel`, UART `0x09000000`), `BoardArm.rxb` (`qemu-system-arm -machine virt -cpu cortex-a15 -kernel`, UART `0x09000000`), and `BoardRv.rxb` (`qemu-system-riscv64 -machine virt -bios none -kernel`, UART `0x10000000`). `CPU.Halt` is `hlt` on x86-64 and `wfi` on the other three. A plain `--target aarch64-unknown-none`, `arm-unknown-none`, or `riscv64-unknown-none` still writes a relocatable object. `Console` stops in the interpreter with the freestanding message. This is not a PC BIOS on ARM, and it is not a boot of the other QEMU system binaries.

`Examples/StageF/Bits.rx` and `Bits.c` both print `8195`, `4096`, `240`, `127`, and `-2` on the interpreter and on the native build. `PageEntry` is `(phys & ~0xFFF) | flags`. The QEMU image rebuilt from that formula still writes byte 82. A 50,000,000-iteration mix loop, `Examples/StageF/BitLoop.rx` and `BitLoop.c`, printed the same checksum `1638366881728`. After one warmup the three Robix times were 60.9, 33.6, and 33.0 ms, and the three C times were 65.6, 36.3, and 39.5 ms. That is one loop on this machine, not a new suite score. The earlier micro-suite ties were not re-run.

## Partial

| Feature | What exists | What does not |
| :--- | :--- | :--- |
| Shared frontend | Lexer, parser, type check, SSA IR for the scalar subset | The interpreter and the native compiler are not proven equivalent by a conformance suite |
| Bytecode interpreter | `Interpreter/VM/BytecodeVM.hpp` and a bytecode module | Not the default proof of language behavior |
| Strings | Literals, concatenation, `==` / `!=`, and `Print` | `.ToString()` is still a native error. Ordering comparisons are not implemented |
| Ranges | Numeric range loops, array index, slices, and `for x in collection` on an array | A stored lambda is not lowered. `throw` does not leave the function |
| C ABI | `extern` with a body is a C-convention definition. `Examples/StageM/AddMain.c` called `Add` | A Robix function that is not `extern` stays `fastcc` |
| Build profiles | `performance`, `realtime`, `safety`, `size`, and host `embedded` select different clang flags. An empty profile stays the release `-O3` path | `embedded` on this machine is a freestanding object. It is not a new board image |
| Targets | Windows x64 hosted link. Kernel projects boot in QEMU for `x86_64-unknown-none`, `aarch64-unknown-none`, `arm-unknown-none`, and `riscv64-unknown-none`. `Examples/StageN/Linux.rxb` links an `x86_64-unknown-linux-gnu` ET_EXEC ELF. The existing `Build/StageF/serial.elf` also booted on QEMU `q35` and `isapc` | That Linux ELF was not executed. A plain `--target` for AArch64, ARM, or RISC-V still writes a relocatable object |
| Benchmarks | `Benchmarks/Micro/README.md`: 12 ties, 0 losses, MemCopy unconfirmed | No manifest sets `upstream: true`. Other suite names are refused |
| Specification | `Specification/LanguageSpecification.md` and grammar files | The documents describe a larger language than this table |

## Unsupported on the native backend

These are not native-complete. A diagnostic is reported where one was actually produced.

- Global constants.
- Garbage collection. There is no collector, and one is not part of the language. `defer` runs its body when the scope exits. `using` runs its block. `Memory.Allocate` / `Memory.Free` are explicit `malloc` / `free`.
- OS intrinsics other than `Memory.Allocate` and `Memory.Free`.
- `GPIO.Read`, `PWM.SetDuty`, `UART.Write`, `SPI.Transfer`, `I2C.Write`, `CAN.Send`, `USB.Write`, `PCIe.Read`, `DMA.Start`, `Sensor.Read`, `Motor.SetSpeed`, `Interrupt.Enable`, and `HardwareTimer.Read` on Windows return 1 and write that the device is not available. They do not return a pin level, transfer a byte, or toggle a motor.
- A public package registry. `robix add` can download from an `http` URL on this machine and check an HMAC-SHA256 publisher key. It does not contact a public registry.
- A UI backend on an operating system other than Windows. `RobixUI/Platform/Linux`, `MacOS`, `Android`, and `iOS` have no source. Screen and Board are presented by the Windows window and by the web page. The framebuffer does not draw rounded corners, and it does not apply `gridColumn` or `inset` while placing pixels. The web page puts those on the element style.
- A public certificate authority. Stage K speaks TLS to `127.0.0.1` with a self-signed `robix-stagek` certificate. `robix network probe` reached this machine's LAN address with a separate self-signed `robix-stagen` certificate. Neither certificate is from a public authority.
- A debugger window. `robix debug --attach` uses the Windows debug API. `robix debug` without `--attach` still records source lines and does not open a window. A normal `robix build` does not emit those line calls.

## Commands that exist

| Command | Status |
| :--- | :--- |
| `robixc File.rx -o App -O3` | Implemented |
| `robixc --target x86_64-unknown-none -O3 Examples/StageF/Serial.rx` | Implemented. QEMU serial byte 82 |
| `robixc --target aarch64-unknown-none`, `arm-unknown-none`, `riscv64-unknown-none`, `x86_64-unknown-linux-gnu` | Object file only. Recorded for `Examples/StageF/Abort.rx` on AArch64, RISC-V, and Linux. `arm-unknown-none` is accepted and is an object unless the project `Output` is `Kernel` or `Firmware` |
| `robixc Examples/StageF/BoardX64.rxb`, `BoardA64.rxb`, `BoardArm.rxb`, `BoardRv.rxb` | Implemented. Each QEMU boot wrote serial byte 82 |
| `robixc Examples/StageF/Serial.rxb` and `Firmware.rxb` | Implemented. Both boot in QEMU and write byte 82 |
| `robixc --version`, `robixc --help`, `robixc --emit-llvm`, `robixc --keep-ir` | Implemented |
| `robix run File.rx` | Interpreter. Recorded on Stage B (`506`), Stage C (`150`, then `40`), and the Stage D guide samples |
| `robix new Name` | The earlier styles wrote `Name/Main.rx` or `Source/Main.rx`. `HelloApp` printed `Hello from Robix`. `Application` through `Custom` printed `1`, `3`, `4`, `5`, `6`, `7`, and `8`. Those style names are no longer accepted |
| `robix new [.] [Name] [Blank\|CLI\|Network]` | Implemented. A piped `Bad:Name`, then `Notes`, then `Blank` rejected the first name and wrote `Build/StageO/Notes`. `robix new . Spot Blank` wrote `Spot.rxproj` in the current directory and did not create a `Spot` folder. `Notes/Source/Program.rx` printed `Hello, World!`. `robix new file Sensor class`, `Tools package`, and `Scratch empty` wrote those files and tests. `Sensor.test.rx` passed on both engines. `Toolbox` is CLI: `robix run Source/Program.rx alpha beta` printed `2`, `alpha`, and `beta`. `robix run Source/Program.rx --native one two` printed `2`, `one`, and `two`. Its test passed on both engines with no arguments. `Wire Network --clients 1 --client Desk GUI --servers 1 --server Hub CLI` wrote `Desk` and `Hub` in the project root, `Common/Source` interfaces only, and `Desk/Desk.rxu`. `.env.common` and `exp.env.common` contain `SECRET=robix-local-secret`, `Desk` on `127.0.0.1:4010`, and `Hub` on `127.0.0.1:4011`. The project root has no lone `.env`. `.gitignore` contains `Build/`. The other env files are empty. `robix run all` in one console printed `[Hub] SendText How are you -> Hello, How are you`, `[Desk] Hello, How are you`, `[Desk] Ask How are you -> How are you`, and `[Hub] How are you`. With `Hub` already listening, changing `SECRET` made `robix run Desk` print `denied`; the original secret printed `Hello, How are you`. `robix run all --separate` opened two consoles and the current console printed both replies. Those console processes were then stopped. `robix ui validate Desk/Desk.rxu --code Desk/Source/Desk.rx` bound `OnClick SendMessage`. `StdLib/Robix/Env.test.rx` passed on both engines. Each engine printed `warning: env ROBIX_ENV_ABSENT not found` and `warning: env ROBIX_ENV_BLANK is empty`. From `Lane`, `Env.Get("NOT_THERE")` printed that same not-found warning, `Exists` printed `0` with no warning, `Add` wrote `LANE_ENV_MARK` into `.env.common`, an empty value printed `warning: env LANE_ENV_MARK is empty`, `Get("SECRET")` printed `robix-local-secret`, and `Remove` restored `.env.common` |
| `robix test Tests/Conformance` | Implemented. Interpreter and native both passed arithmetic, wrap, branches, calls, arrays, structs, and pointers. Bytecode version 99 was refused before execution. `NativeOnly.test.rx` stopped on `Port.Write8` with the freestanding diagnostic. The native-only list is `Tests/Conformance/NATIVE_ONLY.md` |
| `robix test StdLib/Robix` | Implemented. `conformance: both engines passed` for Core, Collections, Math, String, IO, Memory, Atomics, Threading, Hardware, Robotics, and Encoding. `Build/StageH-io.txt` contains `RobixIO`. Windows `GPIO.Read(13)` printed the not-available message on both engines and the test saw error tag 0, value 1. Native `Thread.AddPair` starts two Windows threads. The interpreter applies those two additions on the calling thread and returns the same sum |
| `robix add`, `remove`, `update`, `install`, `restore` | Implemented for a local file. `Examples/StageI` locked `Vendor/Answer.rx` at SHA-256 `d6a6c0a8516b9c10ef7dbb24e3d7245f66a1123ea4187ecb7d078af7f865a070` with an HMAC-SHA256 signature from `SignKey`. A changed file made `restore` exit 1. `install` wrote `Packages/Robix.Answer/Answer.rx`, `Robix.Answer.rxapi`, and `Robix.Answer.rxp` |
| `robix test Examples/StageI` | Implemented. With `RequireTests: true`, a source without `Gap.test.rx` made `robix test` and `robix build` exit 1. After that file was removed, both engines passed `Add.test.rx` and `Main.test.rx`. `Build/StageI-lone/Lone.rx` has no project and no test file; `robix build` produced `Lone.exe`, which printed `1` |
| `robixc App.rxb --explain` and `--dry-run` | Implemented. Printed `order Source/Add.rx Source/Main.rx` and `dry-run: compile was not started`. `Examples/StageF/Kernel.rxb` still explains as `output Kernel`, `entry Kernel.rx`. No output binary was written |
| `robix build Examples/StageI/Source/Main.rx` | Implemented. The second build printed `incremental: unchanged`. Changing `Add.rx`, which `Main.rx` imports, printed `robix build: created executable` and `Build/StageI/stagei.exe` printed `42` |
| `robix ui validate Examples/StageJ/MainWindow.rxu --code Examples/StageJ/Main.rx` | Implemented. Bound `OnClick StartRobot` to `MainController.StartRobot() -> Void` and `StopRobot` the same way. `BadWindow.rxu` exited 1: `SetSpeed` is `(Int) -> Void`, and `Missing` names no function |
| `robix ui render Examples/StageJ/MainWindow.rxu -o Build/StageJ/light.bmp` | Implemented. Software BMP renderer. Those pixels were recorded before the foundation: 0,0 was `F4F5F7` and the Start button at 18,44 was `1D4E89`. `--theme dark` wrote 0,0 as `14161A` and the button as `4C8DFF`. `--width 200` placed `StopButton` at y 80. A later render uses the foundation colors recorded in the foundation row |
| `Examples/UiDemo/RunDemoWindows.ps1` and `RunDemoWeb.ps1` | Both use `robix ui --dev` and the interpreter. The Windows process printed `window: open 320x180 title Robix UI Demo` and `window: development watching`, and was still running after the paint. Changing the window title printed `window: updated`. The web process served `http://127.0.0.1:4173/`. The page contained `Start Robot` and `Robix UI Demo`. After the title changed, `/robix-dev` returned `2` and the page contained `Robix UI Live`. The interpreter printed `UI demo ready` again on that change. `robix ui window` without `--dev` still prints `window: closed after paint` |
| Window resize | The framebuffer is rebuilt for the new client size and then painted across that client. A live `robix ui window Examples/UiDemo/MainWindow.rxu --dev` window was moved to an outer size of 980 by 720. The client became 964 by 681 and the log printed `framebuffer resized 964x681` |
| Window events | The Windows host handles the same window messages as the RefCode host: DPI, mouse move, leave, down, up, double-click, Tab focus, Return and Space, mouse wheel, size, paint, and close. `robix ui window Examples/UiDemo/MainWindow.rxu --code Examples/UiDemo/Main.rx` painted the 1560×1400 framebuffer and printed `window: closed after paint`. The web page listens for pointer, double-click, wheel, key, resize, and hash changes. A live page posts those to `/robix-event`. `robix run RobixUI/Web/Window.rx` printed `open`, `/`, `resize`, `1560`, `1400`, `down`, `up`, `dblclick`, `wheel`, `-120`, `keydown`, `Enter`, `keyup`, and `close`. Text-field editing, the RefCode scrollbar, and the Direct3D swap chain are not in this host |
| Controls | Button, IconButton, Switch, SegmentedControl, ConfirmAction, ThemeToggle, and ThemeIconToggle paint on Windows and on the web. `Build/UiDemo/Controls.rxu` laid out a primary button at 84×44 whose origin pixel is `0E7AFF`, a danger button whose pixel is `FF3B30`, an icon button at 44×44, a checked switch at 112×26, a segmented control at 109×44, confirm at 81×32, theme toggle at 134×36, and theme icon toggle at 96×44. The page has `data-variant="danger"`, `aria-checked="true"`, and `robo-segment is-selected`. Those component folders were removed from RefCode. The framebuffer fills the button as a rectangle and draws the icon button and switch thumb as discs. It does not draw an icon-pack glyph, a spinner animation, or the focus halo |
| Text roles | Text, Heading, Subhead, Body, Paragraph, Caption, Eyebrow, and Label share one renderer. `Build/UiDemo/Type.rxu` laid out Heading 120×59, Subhead 85×31, Body sm 244×24, a Paragraph wrapped to 180×182, Caption 44×15, Eyebrow 41×15, Label 48×17, and Text plus a red Span at 100×26. The bitmap has 88 red span pixels. The page uses `h1.robo-type-h1`, eyebrow `text-transform:uppercase`, and `<span class="robo-span" style="color:#FF3B30">`. Those component folders were removed from RefCode. The framebuffer does not apply letter-spacing; the web style does |
| Foundation fonts | A `Font` entry in `Foundation.rxu` is the font. `font="custom"` with `Src="Custom/Custom.ttf"` rendered `font Custom/Custom.ttf`. That test entry was removed; the shipped foundation still lists the five Inter faces. `system`, `serif`, and `mono` still select Segoe UI, Georgia, and Consolas when the foundation has no font by that name. `RefCode/Components/Screen` and `RefCode/Components/Board` have been removed |
| Inter fonts | The default face is Inter. `Assets/Fonts/Inter` has `Inter-Regular.ttf`, `Inter-Medium.ttf`, `Inter-SemiBold.ttf`, `Inter-Bold.ttf`, and `Inter-Italic.ttf`. `font` chooses a face the way the compiler does: `sans`, `sans-bold`, `Inter-Medium`, `system`, `serif`, and `mono`. `Examples/UiDemo/Font.rxu` rendered with `font Inter/Inter-Regular.ttf`, `font Inter/Inter-Bold.ttf`, and `font Segoe UI`. The bitmap has 1255 pixels that are not the white background. `Build/UiDemo/font.html` loads `../../Assets/Fonts/Inter/Inter-Bold.ttf` at weight 700, and the bold line is `font-weight:700`. A missing family name warns and the Windows paint uses Inter Regular |
| Screen and Board | Implemented for web and Windows. `Screen.rxu` and `Board.rxu` are the same file under `RobixUI/Components`, `RobixUI/Web`, and `RobixUI/Platform/Windows`, with no XML prolog. A screen with no size is 640 by 400 and theme dark: `Examples/UiDemo/Default.rxu` rendered `layout Screen Main 0 0 640 400` and pixel 0,0 `000000`. The demo screen is 640 by 400, theme light, with `MainBoard` filling it and `Actions` as a row. Start is at 16,42 and 104 by 26; Stop is at 132,42, inside the 212-wide row. `robix ui web` wrote `Build/UiDemo/screen.html` with `class="robo-screen"`, `data-place="center"`, `data-theme="light"`, `data-movable="true"`, and `class="robo-board"`. `robix ui window` printed `screen place center 640 340 640x400`, `screen state normal movable true resizable true`, showed the framebuffer, and closed after paint. The Windows screen is a popup; dragging uses the client area when `movable` is true, and `resizable` adds a thick frame. An older `Window` document still renders |
| `robix ui render` and `robix ui window` framebuffer | The fill, 3x5 glyph paint, and BMP byte order live in `RobixUI/Platform/Framebuffer.cpp`. `robix ui render Examples/UiDemo/MainWindow.rxu -o Build/UiDemo/framebuffer.bmp` printed `renderer platform-framebuffer` and the same pixels as the previous foundation BMP: origin `F5F5F7`, title `1D1D1F`, Start button `0E7AFF`. The two BMP files have the same SHA-256. `robix ui window` printed `framebuffer 320x180`, showed that buffer through `CopyToDisplay`, and closed after paint. Linux, MacOS, Android, and iOS do not call it |
| `robix ui web` and `robix ui window` with `Foundation.rxu` | Implemented for web and Windows. `RobixUI/Components/Foundation.rxu`, `RobixUI/Web/Foundation.rxu`, and `RobixUI/Platform/Windows/Foundation.rxu` are the same file and have no XML prolog. `robix ui web Examples/UiDemo/MainWindow.rxu -o Build/UiDemo/foundation.html` printed `foundation bg-secondary light F5F5F7 dark 1C1C1E`. The page sets `--bg-secondary:#f5f5f7`, `--text-primary:#1d1d1f`, `--brand-primary:#0e7aff`, `--space-4:16px`, `--body-size:17px`, and the dark values `#1c1c1e`, `#ffffff`, `#3b9eff`. `robix ui window` on the same document printed the same foundation line, showed the window, and closed after paint. `robix ui render` wrote `Build/UiDemo/foundation.bmp`: pixel 0,0 is `F5F5F7`, the title pixel is `1D1D1F`, and the Start button pixel at 18,44 is `0E7AFF`. Linux, MacOS, Android, and iOS still have no foundation file |
| `robix ui window` and `robix ui web` | The first component is Window. `RobixUI/Components/Window.rxu`, `RobixUI/Web/Window.rxu`, and `RobixUI/Platform/Windows/Window.rxu` are the same file and have no XML prolog. `robix ui web Examples/StageJ/MainWindow.rxu -o Build/StageJ/window.html` printed `component Window` and `web Build/StageJ/window.html 320x180 title Roboticela Robot Control`. The page has `data-component="Window"` and `data-theme="light"`. `robix ui window` on the same document printed `component Window`, `window: shown 320x180 title Roboticela Robot Control`, and `window: closed after paint`, then exited 0. Linux, MacOS, Android, and iOS platform folders are empty |
| `robix network check` and `robix network run Examples/StageK` | Implemented for a local project that sets `Network: true`. The contract parser read `GetStatus(Int) -> Int` and `BatteryLow(Int) -> Void`. A call on `127.0.0.1` returned `reply Int 42`, then the event `BatteryLow Int 7`. `String` was rejected with `error expected Int`. A receive timeout fired at 203 ms. Schannel completed a TLS 1.2 handshake whose peer subject is `robix-stagek`, and the same call inside that session returned `reply Int 42`. Two UDP clients then sent `Move` snapshots: 39 applied in 297 ms, player 1 ended at 60,40, player 2 at 120,20, one older sequence was dropped, and `state 1 late no 1` was rejected. This is localhost UDP, not an internet game session. `Examples/StageK-bad` exited 1 because `SERVER_TOKEN` is in both env files. `Build/StageK/client.env` has `APP_NAME` and `PUBLIC_HOST` only |
| `robix build` with and without `Network: true` | Implemented. `Build/StageK-plain/Plain.rx` printed `network-runtime: omitted`, and `Plain.exe` does not contain `rx-network-runtime-marker`. `Examples/StageK/Source/Client.rx` printed `network-runtime: linked Build/obj/NetworkRuntime.obj`. `client.exe` contains that marker and does not contain `STAGEK_SERVER_TOKEN_9f3a` or `STAGEK_CERT_PASSWORD_c4e1`. The executables printed `2` and `1` |
| `robix format`, `robix lint`, `robix check` | Implemented. `format Examples/StageL/Messy.rx --check` exited 1. After `format`, the `Print` line has 4 spaces and `--check` printed `format: already formatted`. `lint Bad.rx --strict` exited 1 with `L0004` on `maxSpeed` and `D3001` for a missing `Explain` block. `lint Clean.rx --strict` exited 0. `check TypeError.rx` exited 1 with `R1001` (`String` assigned to `Int`). `check Clean.rx` exited 0 |
| `robix doc Examples/StageL/Clean.rx` | Implemented. Wrote `Build/StageL/Clean.md` from the parser. `Add` has Description, Input, Processing, and Output. The nested method `Counter.Value` has Description `Read the counter.`, Processing `Return zero.`, and Output `Zero.` |
| `robix debug Examples/StageL/Debug.rx` | Implemented. The interpreter and the debug native build both reported source lines `4 5 3`. `Build/StageL/debug.ll` calls `rx_debug_line`. `robix build` of the same file with `--keep-ir` wrote `Build/StageL/plain.ll` with no `rx_debug_line` call. Without `--attach` this does not open a debugger window |
| `robix debug --attach Examples/StageN/Program.rx` | Implemented on Windows. Printed `debug: attached pid 26036`, `debug: event CREATE_PROCESS_DEBUG_EVENT`, and `debug: event EXIT_PROCESS_DEBUG_EVENT code 0`. The process printed `9` and the command exited 0. No debugger window was opened |
| `robix lsp` | Implemented. `Examples/StageL/LspClient.ps1` sent Content-Length JSON-RPC for `initialize`, `textDocument/didOpen`, `textDocument/hover`, and `shutdown`. Hover on `Value` returned `Read the counter.`, Processing `Return zero.`, and Output `Zero.`. `Editors/vscode` was packaged as `Build/StageN/robix.vsix` and `cursor --install-extension` installed `roboticela.robix`. Opening `Examples/StageN/Program.rx` in Cursor set the language to Robix and the extension's `activate` wrote `Build/StageN/extension-activated.txt` |
| `robix repl` | Implemented. A piped session of `Print(40 + 2);` then `run` printed `42` and exited 0. The line is written to `Build/StageM/Repl.rx` and checked with the same parser and type checker as `robix run` |
| `Examples/StageM/Features.rx` | Implemented on both engines. Printed `6`, `3`, `9`, `7`, `10`, `11`, `42`, `12`, `41`, `42`, `4`. That is an array `for`, `match` on `Error(3)` and `Success(9)`, `try` / `throw 7` / `catch`, `defer` printing `11` after `10`, an immediate lambda that captured `base` and printed `42`, `using` printing `12`, and `Pair` implementing `Alpha` and `Beta` (`41` and `42`). Stderr on both engines said `PWM.SetDuty`, `UART.Write`, `SPI.Transfer`, and `I2C.Write` are not available, and their return values summed to `4`. A lambda stored in a variable is not lowered. `throw` does not leave the function |
| `robixc Examples/StageM/Export.rx --no-main -o Build/StageM/Export.obj` | Implemented. `Build/StageM/Export.ll` contains `define dso_local i64 @Add`. `clang Examples/StageM/AddMain.c Build/StageM/Export.obj -o Build/StageM/add.exe` printed `42` |
| `robixc --profile performance`, `realtime`, `safety`, `size`, `embedded` | Implemented as separate link modes. The recorded flags were `-O3 -funroll-loops` for performance, `-O2 -fno-strict-aliasing` for realtime, `-O0` for safety, `-Oz` for size, and `-ffreestanding -O3` for embedded. Each log printed `profile:` and that name. `p-performance.exe`, `p-safety.exe`, `p-size.exe`, and `p-realtime.exe` printed `6`. `p-embedded.o` is a host object, and the log said no cross linker was run. `robixc` with no profile printed no `profile:` line, and `p-default.exe` printed `6` |
| `Examples/StageM/Large.rx` | Implemented. 829 lines: `Point`, `Counter`, and `Step0` through `Step159`. The interpreter and `large.exe` both printed `12733`. No compiler change was made for that file |
| `Examples/StageN/Slots.rx` | Implemented on both engines. `Plain.Mid` printed `11` and `WithExtra.Mid` printed `22`. `WithExtra` also implements `Extra.Aaa`, which is a different interface slot. The second call did not run `Aaa` |
| `Examples/StageN/Devices.rx` | Implemented on both engines. `CAN.Send`, `USB.Write`, `PCIe.Read`, `DMA.Start`, `Sensor.Read`, `Motor.SetSpeed`, `Interrupt.Enable`, and `HardwareTimer.Read` each printed that the device is not available. The eight return values summed to `8`. No pin, bus, or motor was used |
| `Examples/StageN/Executable.rxb` | Implemented. `program.exe` printed `9` |
| `Examples/StageN/Library.rxb` and `StaticLibrary.rxb` | Implemented. Both are `llvm-ar` archives. `clang` linked `Caller.c` with `add-lib.a` and with `add-static.a`. Both programs printed `42` |
| `Examples/StageN/SharedLibrary.rxb` | Implemented. `llvm-readobj --coff-exports` showed export `Add`. `LoadDll.c` loaded `Build/StageN/add.dll` and printed `42` |
| `Examples/StageN/Object.rxb` | Implemented. `clang` linked `Caller.c` with `add.obj` and the program printed `42` |
| `Examples/StageN/Package.rxb` | Implemented. `add.rxlib` starts with `RXPKG 1` and `sha256 c9ab418f8f7ffe8102661e9bec7492c3c2e97d5dd1f401c8273527750ce2eca0` |
| `Examples/StageN/Linux.rxb` | Linked. `llvm-readobj -h` on `linux.elf` reported Type `Executable (0x2)`, Machine `EM_X86_64 (0x3E)`, and entry `0x400000`. The file was not executed. No user-mode QEMU binary was available |
| QEMU `q35` and `isapc` | The existing `Build/StageF/serial.elf` was booted with `qemu-system-x86_64 -machine q35` and `-machine isapc`. Each serial file was one byte, `82`. These are not new architecture images, and the four earlier boards were not re-run |
| `robix add Remote http://127.0.0.1:8765/Remote.rx --publisher-key stageN-publisher --project Examples/StageN/Registry.rxproj` | Implemented against a local HTTP server on this machine. Printed `robix add: downloaded Remote publisher signature matched` and SHA-256 `4b8948273d97a766cbdff9b9e28eab772be467ac3094ed9c26001e6502a07421`. The cached file is `Examples/StageN/Packages/cache/Remote.rx`. The same URL with `--publisher-key wrong-key` printed `publisher signature does not match` and exited 1. This is not a public registry |
| `robix network probe` | Implemented on this machine. Printed `probe: connected 10.217.223.118 reply pong` and `probe: certificate robix-stagen`. The certificate is self-signed on this machine. It is not a public certificate |
| `Editors/vscode` | Installed in Cursor as `roboticela.robix` from `Build/StageN/robix.vsix`. Opening `Examples/StageN/Program.rx` showed the Robix language mode and wrote `Build/StageN/extension-activated.txt`. The activation write was then removed from `extension.js` |

## RoboticelaOS Stage A

`Frame.Create`, `Frame.Fill`, `Frame.Pixel`, `Frame.Present`, and `AssertPixel` run on the interpreter and in a native image. On a freestanding image, `Frame.Present` programs the Bochs display interface to 320×200×32, reads the display PCI BAR, copies the frame there, and returns the first framebuffer byte.

```
robix test RoboticelaOS/Tests/RoboFrame.test.rx
robix test RoboticelaOS/Runtime/GPU/RoboGPU.test.rx
robix test RoboticelaOS/Kernel/RoboKernel.test.rx
```

Each printed `interpreter ok, native ok`.

```
robixc -O3 RoboticelaOS/Kernel/RoboKernel.rxb -o Build/RoboticelaOS/RoboKernel.elf
qemu-system-x86_64 -cpu qemu64 -machine pc -kernel Build/RoboticelaOS/RoboKernel.elf -serial file:Build/RoboticelaOS/stageA.serial -display none -vga std
```

The serial file was:

```
ROBO_GPU: present
ROBO_GPU: pixel
```

A QEMU `screendump` wrote `Build/RoboticelaOS/stageA.ppm` at 320×200. Pixel (0, 0) is `0E 7A FF`. Pixel (0, 40) and pixel (319, 199) are `08 0A 10`. Pixel (16, 8) is `FF FF FF`. Pixel (24, 8) is `0E 7A FF`. The logo is an 8×8 white rectangle. The identity map covers the first 4GB, and unwind tables are discarded so `Start` stays at 2MB.

## RoboticelaOS Stage B

`robix ui cook` lays the document out with the Windows host and writes `RXUL 1` records: kind, x, y, width, height, text. `UI.LoadCooked`, `UI.Role`, `UI.BoxX`, `UI.BoxY`, `UI.BoxWidth`, and `UI.BoxHeight` read that blob. Role numbers are Bar 1, Icon 2, Window 3, Button 4, Text 5, Screen 6. A freestanding image embeds the blob at compile time. The interpreter reads the file.

```
robix ui cook RoboticelaOS/Tests/RoboCookButton.rxu -o RoboticelaOS/Tests/RoboCookButton.rxul
robix ui cook RoboticelaOS/Desktop/RoboDesktop.rxu -o RoboticelaOS/Desktop/RoboDesktop.rxul
robix test RoboticelaOS/Tests/RoboCook.test.rx
robix test RoboticelaOS/Runtime/UI/RoboUI.test.rx
robix test RoboticelaOS/Kernel/RoboKernel.test.rx
```

The button blob is `Button 0 0 84 44 Open`. The desktop blob has the screen, a 320×32 bar, three 32×32 icons, a 240×88 window, and the title text. Each test printed `interpreter ok, native ok`.

```
robix ui validate RoboticelaOS/Desktop/RoboDesktop.rxu
robix ui window RoboticelaOS/Desktop/RoboDesktop.rxu
```

Validate printed `screen validated Desktop\RoboDesktop.rxu 320x200`. The window printed `window: shown 320x200 title RoboticelaOS` and `window: closed after paint`.

```
robixc -O3 RoboticelaOS/Kernel/RoboKernel.rxb -o Build/RoboticelaOS/RoboKernel.elf
qemu-system-x86_64 -cpu qemu64 -machine pc -kernel Build/RoboticelaOS/RoboKernel.elf -serial file:Build/RoboticelaOS/stageB.serial -display none -vga std
```

The serial file was:

```
ROBO_GPU: present
ROBO_UI: paint
ROBO_GPU: pixel
```

`Build/RoboticelaOS/stageB.ppm` is 320×200. Pixel (0, 0) and pixel (200, 8) are `0E 7A FF`. Pixel (0, 199) is `08 0A 10`. Pixels (16, 48), (64, 48), and (112, 48) are `FF FF FF`. Pixel (276, 180) is `1C 24 30`. `robix run` from `RoboticelaOS` printed the same three serial lines and wrote `Build/RoboticelaOS/Interpreter.bmp` with those pixels.

## RoboticelaOS Stage C

`Input.Play` reads a script of `move`, `click`, and `key` records separated by `;`. The records follow `RoboInputEvent`: key is type 1, mouse is type 2, and the button bit is the mouse value. The queue holds 128 events. `Input.Count`, `Input.X`, `Input.Y`, and `Input.Button` read it. `Pop` removes the oldest event. Relative, absolute, and key pushes append. `PollPS2` reads port 100 and, only when bit 0 is set, reads port 96 and pushes that byte as a key. On the interpreter those ports read as 0, so a quiet controller does not invent a packet.

```
robix test RoboticelaOS/Tests/RoboPlay.test.rx
robix test RoboticelaOS/Kernel/Input/RoboInput.test.rx
robix test RoboticelaOS/Runtime/UI/RoboHit.test.rx
robix test RoboticelaOS/Kernel/RoboKernel.test.rx
```

Each printed `interpreter ok, native ok`. The play test checks a move at (16, 48) with button bit 0 and a click at (80, 140) with button bit 1. The queue test pops the move and then sees the click. The hit test maps (16, 48) to an icon and (200, 160) to the window. A click with button bit 0 does not count. Button bit 1 on the window makes `Input.Clicks` 1. The same bit on the icon leaves the count at 1.

```
robixc -O3 RoboticelaOS/Kernel/RoboKernel.rxb -o Build/RoboticelaOS/RoboKernel.elf
qemu-system-x86_64 -cpu qemu64 -machine pc -kernel Build/RoboticelaOS/RoboKernel.elf -serial file:Build/RoboticelaOS/stageC.serial -display none -vga std
```

The serial file was:

```
ROBO_GPU: present
ROBO_UI: paint
ROBO_UI: click
ROBO_GPU: pixel
```

`Build/RoboticelaOS/stageC.ppm` is 320×200. Pixel (0, 0) is `0E 7A FF`. Pixel (120, 24) is `FF FF FF`, the cursor after the move. Pixel (16, 48) is `FF FF FF`. Pixel (276, 180) is `FF B0 00`, the window after the click. Pixel (0, 199) is `08 0A 10`. `robix run` from `RoboticelaOS` printed the same serial lines and wrote `Build/RoboticelaOS/Interpreter.bmp` with those pixels.

## RoboticelaOS Stage D

`Scene.Render` draws one `.rx3d` scene into a frame. The camera is perspective, Y is up, and rotation is radians. One directional light shades flat box meshes. The freestanding image copies that frame with volatile stores so the optimizer does not emit `memcpy`.

```
robix test RoboticelaOS/Tests/RoboSplash.test.rx
robix test RoboticelaOS/Runtime/Scene/RoboSceneRaster.test.rx
robix test RoboticelaOS/Runtime/UI/RoboUI.test.rx
robix test RoboticelaOS/Kernel/RoboKernel.test.rx
```

Each printed `interpreter ok, native ok`. The splash center pixel (160, 100) is `3300229`. The splash corner (0, 0) and the wallpaper corner (0, 199) are `526864` (`#080A10`). The wallpaper body pixel (300, 160) is `3155991`.

```
robixc -O3 RoboticelaOS/Kernel/RoboKernel.rxb -o Build/RoboticelaOS/RoboKernel.elf
qemu-system-x86_64 -cpu qemu64 -machine pc -kernel Build/RoboticelaOS/RoboKernel.elf -serial file:Build/RoboticelaOS/stageD.serial -display none -vga std
```

The serial file was:

```
ROBO_GPU: present
ROBO_UI: paint
ROBO_SCENE: frame
ROBO_UI: click
ROBO_GPU: pixel
```

`Build/RoboticelaOS/stageD.ppm` is 320×200. Pixel (0, 0) is `0E 7A FF`. Pixel (120, 24) is `FF FF FF`. Pixel (16, 48) is `FF FF FF`. Pixel (300, 160) is `30 28 17`, the wallpaper body beside the window. Pixel (276, 180) is `FF B0 00`, the clicked window. Pixel (0, 199) is `08 0A 10`. `robix run` from `RoboticelaOS` printed the same serial lines and wrote `Build/RoboticelaOS/Interpreter.bmp` with those pixels. `Desktop/RoboDesktop.rxu` references the scene with `View3D`, and the scene file has no RobixUI elements.

## RoboticelaOS Stage E

`UI.Place`, `UI.Hit`, `UI.Drag`, and `UI.Snap` return a new layout and a cursor kind. `UI.LoadCooked` copies the cooked records back and clears focus and the menu, so a later bar hit still sees the bar after a snap. That copy uses volatile stores. A plain copy became `memcpy` and the freestanding link failed.

```
robix test RoboticelaOS/Tests/RoboPlace.test.rx
robix test RoboticelaOS/Runtime/UI/RoboWindow.test.rx
robix test RoboticelaOS/Runtime/UI/RoboShell.test.rx
robix test RoboticelaOS/Runtime/GPU/RoboCursor.test.rx
robix test RoboticelaOS/Runtime/GPU/RoboText.test.rx
robix test RoboticelaOS/Kernel/RoboKernel.test.rx
```

Each printed `interpreter ok, native ok`. Dragging window 6 by `(12, -8)` returns kind 2 and moves the origin to `(52, 88)`. Snapping that window to the left half returns kind 6, with `x = 0`, width `160`, and height `200`. `UI.Hit(60, 108)` is kind 4 on the title text. `UI.Hit(40, 140)` is kind 6 on the window edge. `OpenBar(8, 8)` opens the menu. `OpenBar(200, 180)` leaves it closed. `DrawClock` for time `1032` lights 44 pixels.

```
robixc -O3 RoboticelaOS/Kernel/RoboKernel.rxb -o Build/RoboticelaOS/RoboKernel.elf
qemu-system-x86_64 -cpu qemu64 -machine pc -kernel Build/RoboticelaOS/RoboKernel.elf -serial file:Build/RoboticelaOS/stageE.serial -display none -vga std
```

The serial file was:

```
ROBO_GPU: present
ROBO_UI: paint
ROBO_SCENE: frame
ROBO_UI: click
ROBO_SHELL: bar
ROBO_GPU: miss
```

`ROBO_GPU: miss` is the first framebuffer byte after the snap fill. Pixel `(0, 0)` is the window color `1C 24 30`, so that byte is `0x30`.

`Build/RoboticelaOS/stageE.ppm` is 320×200. Pixel `(0, 0)` is `1C 24 30`, the snapped window. Pixel `(8, 40)` is `2A 23 44`, the menu (`2761540`). Pixel `(200, 8)` is `0E 7A FF`, the bar. Pixel `(221, 8)` and pixel `(224, 8)` are `FF FF FF`, clock glyphs for `1032`. Pixel `(220, 8)` is `0E 7A FF`, a clock gap showing the bar. Pixel `(250, 16)` is `FF FF FF`, the cursor. Pixel `(300, 160)` is `30 28 17`, the wallpaper. Pixel `(0, 199)` is `1C 24 30`. `robix run` from `RoboticelaOS` printed the same serial lines and wrote `Build/RoboticelaOS/Interpreter.bmp` with those pixels.

## RoboticelaOS Stage F

`@Packed` lays a struct out with no padding. `@Align(N)` forces a power-of-two alignment and rounds the size up to it. `@Size(N)` fails the build when the computed size differs. `Memory.SizeOf` and `Memory.OffsetOf` fold to integer constants in both engines. A top-level `const` with an integer value does the same, including a `const` imported from another file.

```
robix test RoboticelaOS/Tests/RoboLayout.test.rx
robix test RoboticelaOS/Shared/RoboTypes.test.rx
robix test RoboticelaOS/Shared/RoboHandoff.test.rx
robix test RoboticelaOS/Shared/RoboPacks.test.rx
robix test RoboticelaOS/Boot/RoboBootloader.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboSerial.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboSplash.test.rx
robix test RoboticelaOS/Kernel/RoboKernel.test.rx
```

Each printed `interpreter ok, native ok`. The packed stand-in has `magic` at offset 0 and `version` at offset 4, and its size is 8. A packed `UInt8` then `UInt32` is 5 bytes with the second field at offset 1. The same fields without `@Packed` put the second field at offset 4 and the size at 8. `@Align(8)` on a one-byte struct makes the size 8. `ROBO_HANDOFF_MAGIC` is `0x444E4852`. `RoboHandoff` is 1448 bytes. `tscFrequency` is at offset 192. `memoryRanges` is at offset 248. `RoboMemoryRange` is 24 bytes and its kind field, `rangeType`, is at offset 16. `type` is a Robix keyword, so that field keeps the C offset under the name `rangeType`. `RoboU8` through `RoboU64` are 1, 2, 4, and 8. Pack magic `0x52554352` (`RCUR`), `0x50434952` (`RICP`), and `0x43494F52` (`ROIC`) are accepted. Magic `0` and magic `1` are rejected. The map fixture is type 1 then type 2. A ceiling never moves backward: `Advance(100000, 97400)` stays `100000`. The boot line starts with byte 82 and the word `handoff`.

```
robixc RoboticelaOS/Tests/RoboLayoutBad.rx -o Build/RoboticelaOS/RoboLayoutBad.exe
```

The build failed with `L1008`: the struct is 8 bytes, and `@Size` asked for 7.

```
robixc -O3 RoboticelaOS/Kernel/RoboKernel.rxb -o Build/RoboticelaOS/RoboKernel.elf
qemu-system-x86_64 -cpu qemu64 -machine pc -kernel Build/RoboticelaOS/RoboKernel.elf -serial file:Build/RoboticelaOS/stageF.serial -display none -vga std
```

The serial file was:

```
ROBO_BOOT: handoff
ROBO_KERNEL: start
ROBO_GPU: present
ROBO_UI: paint
ROBO_SCENE: frame
ROBO_UI: click
ROBO_SHELL: bar
ROBO_GPU: miss
```

`Build/RoboticelaOS/stageF.ppm` is 320×200 and matches the Stage E desktop. Pixel `(0, 0)` is `1C 24 30`. Pixel `(8, 40)` is `2A 23 44`. Pixel `(200, 8)` is `0E 7A FF`. Pixel `(221, 8)` is `FF FF FF`. Pixel `(250, 16)` is `FF FF FF`. Pixel `(300, 160)` is `30 28 17`. Pixel `(0, 199)` is `1C 24 30`. The bootloader writes the handoff at `458752` (`0x70000`): magic, version 4, two memory ranges, and a TSC frequency measured from the PIT. The splash bar is drawn and then the desktop paint covers it.

```
robixc -O3 RoboticelaOS/Boot/RoboBadHandoff.rxb -o Build/RoboticelaOS/RoboBadHandoff.elf
qemu-system-x86_64 -cpu qemu64 -machine pc -kernel Build/RoboticelaOS/RoboBadHandoff.elf -serial file:Build/RoboticelaOS/stageF-bad.serial -display none -vga std
```

That serial file was `ROBO_KERNEL: bad handoff`. The image stores magic 0 and halts.

`Boot/RoboBootROM.s` is the 16-bit reset. It linked to `Build/RoboticelaOS/RoboBootROM.bin`, 65536 bytes. QEMU `-bios` wrote serial byte 73 (`I`). `robix run` from `RoboticelaOS` printed the same kernel serial lines and wrote `Build/RoboticelaOS/Interpreter.bmp` with the same pixels. On the host, `Port.Write8` records only the serial port `1016`. The PIT ports stay off that log.

The boot screen follows the RefCode layout on a 1920×1080 screen, the size RefCode picks first so a 16:9 display is not stretched. The logo is 187×216, one fifth of the screen height, and the loader is a 422×6 capsule, 22 percent of the screen width. The fill advances one pixel at a time and takes the same four seconds on every boot. Two QEMU samples one second apart put the fill edge at x=944 and then x=1125. `Build/RoboticelaOS/stageF-desktop.ppm` is the Stage E desktop after that. `RoboticelaOS/RunQEMU.ps1` opens the window.

## RoboticelaOS Stage G

`Page.Entry(phys, flags)` is `(phys & ~0xFFF) | flags`. `Memory.Frame()` hands out one 4 KiB frame from a pool of eight. On the host the addresses start at `2097152`. The ninth call returns 0. In the freestanding image the frames come from the identity-mapped page pool, and the ninth call returns 0 instead of trapping.

```
robix test RoboticelaOS/Tests/RoboBits.test.rx
robix test RoboticelaOS/Kernel/Memory/RoboMemoryManager.test.rx
robix test RoboticelaOS/Kernel/RoboKernel.test.rx
```

Each printed `interpreter ok, native ok`. `Page.Entry(0x2000, 0x3)` is `8195`. The other four bit values are `4096`, `240`, `127`, and `-2`. The first 1 GiB identity entry is `131`. Slot 1 is `1073741955`. Slot 3 is `3221225603`. Boot, runtime, and ACPI ranges stay reserved and return frame 0. A free range past its page count returns 0. The first host frame is `2097152` and the next is `2101248`.

```
robixc -O3 RoboticelaOS/Kernel/RoboKernel.rxb -o Build/RoboticelaOS/RoboKernel.elf
qemu-system-x86_64 -cpu qemu64 -machine pc -kernel Build/RoboticelaOS/RoboKernel.elf -serial file:Build/RoboticelaOS/stageG.serial -display none -vga std
```

The serial file was:

```
ROBO_BOOT: handoff
ROBO_KERNEL: start
ROBO_MEMORY: pages
ROBO_MEMORY: map
ROBO_BOOT: splash
ROBO_GPU: present
ROBO_UI: paint
ROBO_SCENE: frame
ROBO_UI: click
ROBO_SHELL: bar
ROBO_GPU: miss
```

`InstallMap` writes four 1 GiB identity entries, loads that root with `CPU.WriteCR3`, and `CPU.ReadCR3` matches it. The splash bar is drawn at 97, the memory ceiling `97400`. `Build/RoboticelaOS/stageG.ppm` is 320×200. Pixel `(0, 0)` is `1C 24 30`, the desktop after the splash.

## RoboticelaOS Stage H

`CPU.Gate` builds the interrupt descriptor. For handler `305419896` the low word is `1311829522123347576` and the high word is `0`. For handler `4294967297` the low word is `156130651668481` and the high word is `1`. `Clock.Seconds(66)` is `42`. `Volatile.Store32` of `1` through a pointer reads back as `1`.

```
robix test RoboticelaOS/Kernel/Interrupt/RoboIDT.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboTimer.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboScreen.test.rx
```

Each printed `interpreter ok, native ok`.

```
robixc -O3 RoboticelaOS/Boot/RoboTrap.rxb -o Build/RoboticelaOS/RoboTrap.elf
qemu-system-x86_64 -cpu qemu64 -machine pc -kernel Build/RoboticelaOS/RoboTrap.elf -display none -no-reboot -serial file:Build/RoboticelaOS/qemu.trap
```

The trap serial file is exactly `ROBO_IDT: trap 3`.

## RoboticelaOS Stage I

Installed RAM comes from CMOS registers `0x34` and `0x35`. On this QEMU machine that is 128 MiB, serial `ROBO_MEMORY: ram 128`. The free range starts at 16 MiB and the identity map covers that RAM plus the local APIC slot. The boot processor brings the other cores up after the splash. Each online core fills its own rows of the 1920×1080 framebuffer from its own stack at the top of the measured RAM. On this 128 MiB machine the first application stack is `134201336`. The boot processor paints the bar and the logo after those rows are done. A missing core does not stop the desktop.

```
robix test RoboticelaOS/Boot/RoboBootloader.test.rx
robix test RoboticelaOS/Kernel/Memory/RoboMemoryManager.test.rx
robix test RoboticelaOS/Kernel/CPU/RoboCPU.test.rx
robix test RoboticelaOS/Runtime/GPU/RoboGPU.test.rx
```

Each printed `interpreter ok, native ok`. `RamBytes(0, 7)` is `134217728`. `MapSlots(134217728)` is `1`. `FreePageCount(134217728, 16777216)` is `28672`. `PaintBand` on worker 1 of 4 fills only its rows.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -no-reboot -no-shutdown -vga std -serial file:Build/RoboticelaOS/qemu.serial
```

The four-core serial file was:

```
ROBO_BOOT: handoff
ROBO_KERNEL: start
ROBO_MEMORY: pages
ROBO_MEMORY: map
ROBO_MEMORY: ram 128
ROBO_IDT: gates
ROBO_CLOCK: seconds
ROBO_BOOT: splash
ROBO_CPU: cores online4
ROBO_TIMER: tick
ROBO_GPU: present
ROBO_UI: paint
ROBO_UI: click
ROBO_SHELL: bar
ROBO_GPU: miss
```

The same image with `-smp 1` prints `ROBO_MEMORY: ram 128` and `ROBO_CPU: cores online1`, then reaches `ROBO_SHELL: bar`. `Build/RoboticelaOS/stageRam.ppm` is 1920×1080. Pixel `(0, 0)` is `1C 24 30`. Pixel `(200, 8)` is `0E 7A FF`. Pixel `(960, 540)` is `08 0A 10`. The 320×200 wallpaper scene is not drawn on this frame, so `ROBO_SCENE: frame` is absent.

## RoboticelaOS Stage J

`Sched.Enqueue`, `Sched.Pick`, and `Sched.Block` are the run queue. Three threads enqueued at priorities 1, 5, and 3 are picked as 2, then 3, then 1. A blocked thread stays off the pick until `Enqueue` wakes it. `PickFor` returns 0 for a halted core. `Moved(0, 1)` is 1. The splash ceiling for the scheduler is `98400`.

```
robix test RoboticelaOS/Kernel/CPU/RoboScheduler.test.rx
robix test RoboticelaOS/Kernel/CPU/RoboCPU.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboSplash.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboTimer.test.rx
```

Each printed `interpreter ok, native ok`.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -no-reboot -vga std -serial file:Build/RoboticelaOS/qemu.serial
```

After the desktop lines, the four-core serial file contains:

```
ROBO_SCHED: core 0
ROBO_SCHED: core 1
ROBO_SCHED: core 2
ROBO_SCHED: core 3
ROBO_SCHED: migrate
```

The same image with `-smp 1` prints `ROBO_CPU: cores online1`, reaches `ROBO_SHELL: bar`, and then prints `ROBO_SCHED: core 0`.

## RoboticelaOS Stage K

`Page.Split(8192, 1048576)` is `9987`. The low three bits are the supervisor flags `3`, and the next flag byte is the user flags `7`. `Page.Split(8192, 0)` keeps that half supervisor (`8963`). `KernelHalf(8192, 1048576)` is `8195`. `UserHalf(8192, 8388608)` is `8388615`. `Fault.Decide(1)` is `1` (continue). `Fault.Decide(0)` is `0` (halt). `RootsDiffer(16777216, 16781312)` is `1`, and a zero root or two equal roots return `0`.

```
robix test RoboticelaOS/Kernel/Memory/RoboAddressSpace.test.rx
robix test RoboticelaOS/Kernel/Memory/RoboMemoryManager.test.rx
robix test RoboticelaOS/Kernel/CPU/RoboScheduler.test.rx
robix test RoboticelaOS/Kernel/CPU/RoboCPU.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboSplash.test.rx
```

Each printed `interpreter ok, native ok`.

The kernel builds a second root at 16 MiB. The low 18 MiB stay identity-mapped as 2 MiB pages. The page at 8 MiB is user. The kernel pages, including the page at 2 MiB, stay supervisor. A ring-3 store into that supervisor page raises vector 14. The fault stub pops the error code. When the user bit is clear it halts. When the user bit is set it prints the two lines and returns to the kernel. Vector 32 stays installed, so the timer tick still returns.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -no-reboot -no-shutdown -vga std -serial file:Build/RoboticelaOS/qemu.serial
```

After the Stage J lines, the four-core serial file contains:

```
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints `ROBO_CPU: cores online1`, `ROBO_SCHED: core 0`, then `ROBO_VM: fault` and `ROBO_PROCESS: alive`. `Build/RoboticelaOS/stageAddress.ppm` is 1920×1080. Pixel `(0, 0)` is `1C 24 30`. Pixel `(200, 8)` is `0E 7A FF`. Pixel `(960, 540)` is `08 0A 10`.

## RoboticelaOS Stage L

`Port.Create`, `Port.Send`, and `Port.Receive` are the message queue. The kernel creates the four well-known ports: Log is 1, Window is 2, Launch is 3, and Driver is 4. `Port.Create(0)` is a closed port and returns 5. A send of `305419896` returns 4 and the receive returns those same four bytes. A send of `4294967301` keeps the low four bytes, so the receive is 5. A send of 0 is received as 0. An empty receive returns `-2`, and `Sched.Block` on that waiter returns 1. A send to the closed port, and a send to port 0, both return `-1`.

```
robix test RoboticelaOS/Kernel/IPC/RoboPort.test.rx
robix test RoboticelaOS/Kernel/CPU/RoboScheduler.test.rx
robix test RoboticelaOS/Kernel/Memory/RoboAddressSpace.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboSplash.test.rx
```

Each printed `interpreter ok, native ok`.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -no-reboot -no-shutdown -vga std -serial file:Build/RoboticelaOS/qemu.serial
```

After the scheduler lines, the four-core serial file contains:

```
ROBO_SCHED: core 0
ROBO_SCHED: core 1
ROBO_SCHED: core 2
ROBO_SCHED: core 3
ROBO_TIMER: tick
ROBO_SCHED: migrate
ROBO_PORT: round
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints `ROBO_SCHED: core 0`, then `ROBO_PORT: round`, then `ROBO_VM: fault` and `ROBO_PROCESS: alive`.

## RoboticelaOS Stage M

`Proc.Create`, `Proc.Exit`, and `Proc.Wait` are the process table. `Cred.Allow` is the rights check. Process 1 is `RoboLaunch`, created with parent 0 and the root user. Its child is process 2 and a second child is process 3. Exiting the first child with status 7 makes `Proc.Wait` return 7. The second child is still alive, so `Proc.Wait` returns `-2`. Each process has its own page root from `Page.Split`: the supervisor flags are 3 and the user flags are 7, and the two roots differ. The child is also a scheduler thread, so `Sched.Pick` returns that child. Root may `SystemWrite`. A normal user may write and read, and may not `SystemWrite`. A guest may read and may not write. The splash ceiling for processes is `98600`.

```
robix test RoboticelaOS/Kernel/POSIX/RoboProcess.test.rx
robix test RoboticelaOS/Kernel/POSIX/RoboCredential.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboSplash.test.rx
robix test RoboticelaOS/Kernel/CPU/RoboScheduler.test.rx
```

Each printed `interpreter ok, native ok`.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -no-reboot -no-shutdown -vga std -serial file:Build/RoboticelaOS/qemu.serial
```

After the port line, the four-core serial file contains:

```
ROBO_PORT: round
ROBO_PROC: child
ROBO_PROC: wait
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints `ROBO_PORT: round`, then `ROBO_PROC: child` and `ROBO_PROC: wait`.

## RoboticelaOS Stage N

`Syscall.Dispatch(caller, number, arg)` is the gate. Echo, number 0, returns `arg + 4919` (`0x1337`). An unknown number returns `-1`. `File` returns `-1` until the volume exists. `Map` of a kernel address such as `2097152` returns `-1`, and `Map` of the user page `8388608` returns that address. `Draw` returns `-1` for a normal process and returns the argument for the window server, caller 2. A user fault still continues (`Fault.Decide(1)` is 1) and a kernel fault still halts. The splash ceiling for syscalls is `99400`.

```
robix test RoboticelaOS/Kernel/Syscall/RoboSyscall.test.rx
robix test RoboticelaOS/Kernel/Memory/RoboAddressSpace.test.rx
robix test RoboticelaOS/Kernel/POSIX/RoboProcess.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboSplash.test.rx
```

Each printed `interpreter ok, native ok`.

The user thread enters through vector 128. The handler saves the registers, dispatches echo, and returns to that same thread. The thread then stores into a supervisor page. That fault returns to the kernel.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -no-reboot -no-shutdown -vga std -serial file:Build/RoboticelaOS/qemu.serial
```

After the process lines, the four-core serial file contains:

```
ROBO_PROC: wait
ROBO_SYSCALL: echo
ROBO_SYSCALL: verified syscall round-trip dispatch
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints those same four lines after `ROBO_PROC: wait`.

## RoboticelaOS Stage O

`FS.Lookup`, `FS.Create`, `FS.Read`, `FS.Write`, and `FS.List` share one four-slot volume with syscall `File` (number 6). `Home/Note.txt` is path 1 and `System/Launch/RoboLaunch.rxb` is path 2. Creating the note stores byte 78, a missing path returns `-1`, and a fifth file returns `-1`. The splash ceiling for the filesystem is `99200`.

```
robix test RoboticelaOS/Kernel/FileSystem/RoboVFS.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboDisk.test.rx
robix test RoboticelaOS/Kernel/Syscall/RoboSyscall.test.rx
robix test RoboticelaOS/Kernel/Memory/RoboAddressSpace.test.rx
robix test RoboticelaOS/Kernel/POSIX/RoboProcess.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboSplash.test.rx
```

Each printed `interpreter ok, native ok`.

Sector 0 uses the same command sequence as `Examples/Machine/Disk.rx`. The first boot writes byte 78 and resets the machine. The second boot reads that byte back.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -vga std -hda Build/RoboticelaOS/RoboDisk.img -serial file:Build/RoboticelaOS/qemu.serial
```

The four-core serial file contains both boots. After the second `ROBO_FS: mount`:

```
ROBO_FS: mount
ROBO_FS: Note.txt
ROBO_SYSCALL: echo
ROBO_SYSCALL: verified syscall round-trip dispatch
ROBO_VM: fault
ROBO_PROCESS: alive
```

The disk image byte 0 is 78 after QEMU exits. The same image with `-smp 1` prints `ROBO_CPU: cores online1`, `ROBO_FS: Note.txt`, and the same syscall and fault lines, and it does not print `ROBO_SCHED: migrate`.

## RoboticelaOS Stage P

`Port.Read32` and `Port.Write32` are the 32-bit port operations. On the interpreter they stop with the freestanding diagnostic. `PCI.Decode(word0, word8)` returns vendor, device, and class, and returns `-1` when the vendor is `65535`. `HID.Parse` reads a 3-byte boot mouse, a wheel byte, and a touch contact. The splash ceilings stay at `98800` for input and `99000` for drivers.

```
robix test RoboticelaOS/Kernel/PCI/RoboPCI.test.rx
robix test RoboticelaOS/Kernel/Input/RoboHID.test.rx
robix test RoboticelaOS/Kernel/Input/RoboVirtioMouse.test.rx
robix test RoboticelaOS/Kernel/Input/RoboVMMouse.test.rx
robix test RoboticelaOS/Kernel/Input/RoboI2CTouchpad.test.rx
robix test RoboticelaOS/Kernel/Input/RoboXHCI.test.rx
robix test RoboticelaOS/Kernel/Input/RoboInput.test.rx
robix test RoboticelaOS/Kernel/Devices/RoboSplash.test.rx
```

Each printed `interpreter ok, native ok`.

The boot scans bus 0 through the PCI config ports `3320` and `3324`. A missing virtio mouse is left alone. The pointer queue is the input path. The window server is not running, so nothing is sent to the window port.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -vga std -hda Build/RoboticelaOS/RoboDisk.img -serial file:Build/RoboticelaOS/qemu.serial
```

The four-core serial file contains:

```
ROBO_PORT: round
ROBO_PCI: devices 6
ROBO_INPUT: ready
ROBO_PROC: child
ROBO_PROC: wait
ROBO_DISK: sector
ROBO_FS: mount
ROBO_FS: Note.txt
ROBO_SYSCALL: echo
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints `ROBO_CPU: cores online1`, `ROBO_PCI: devices 6`, and `ROBO_INPUT: ready`, and it does not print `ROBO_SCHED: migrate`.

## RoboticelaOS Stage Q

`Driver.Match(device, personalities)` returns the built-in id when one personality serves that class. Two built-in personalities for one class return `-1`. No personality returns `-1`. `Driver.Bind(device, builtIn, installed)` returns the installed package id when that package names the same vendor and device. After the package is removed, the same Intel Ethernet id `0x8086:0x100E` binds to `RoboBasicNet`. No built-in and no package returns `-1`. A vendor display and a Wi-Fi subclass stay published and unmatched. The driver splash ceiling stays at `99000`.

```
robix test RoboticelaOS/Kernel/IO/RoboDriver.test.rx
robix test RoboticelaOS/Kernel/IO/RoboRegistry.test.rx
robix test RoboticelaOS/Kernel/IO/Basic/RoboBasicDisplay.test.rx
robix test RoboticelaOS/Kernel/IO/Basic/RoboBasicStorage.test.rx
robix test RoboticelaOS/Kernel/IO/Basic/RoboBasicNet.test.rx
robix test RoboticelaOS/Kernel/PCI/RoboPCI.test.rx
robix test RoboticelaOS/Kernel/Input/RoboInput.test.rx
```

Each printed `interpreter ok, native ok`. The other `RoboBasic*.test.rx` files did the same.

The registry publishes every present PCI device. The binder runs before the first user process. On this machine the bound classes are storage, display, and ethernet.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -vga std -hda Build/RoboticelaOS/RoboDisk.img -serial file:Build/RoboticelaOS/qemu.serial
```

The four-core serial file still has `ROBO_UI: click` before the driver lines:

```
ROBO_UI: click
ROBO_PCI: devices 6
ROBO_INPUT: ready
ROBO_DRIVER: basic
ROBO_DRIVER: storage
ROBO_DRIVER: display
ROBO_DRIVER: net
ROBO_FS: Note.txt
ROBO_SYSCALL: echo
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints `ROBO_CPU: cores online1` and the same three class lines, and it does not print `ROBO_SCHED: migrate`.

## RoboticelaOS Stage R

`Driver.Package` takes one word: device bits 0–15, vendor bits 16–31, version bits 32–39, HMAC bits 40–47, magic `21072` in bits 48–63. HMAC is `(21072 xor version xor vendor xor device) and 255`. A bad HMAC returns `-1` and leaves the installed version. Version 1 installs and returns `1`. Version 2 replaces it and returns `65538` (previous version in the high half). `PackageRollback` restores version 1. `Driver.Newer` is true only when the offered version is greater. `Vendor.Match(1, 32902, 40288, 12)` is family 6, the LPSS id `0x9D60`. An unknown Intel id and vendor `4130` (`0x1022`) return `0`. `Driver.Bind` on that LPSS word returns built-in `12`, then package `36` after install, then `12` after remove.

QEMU's `pc` machine has no LPSS device `8086:9D60`. The live Intel device on this bus is the e1000 at `8086:100E` (device `4110`, Ethernet family 3). Install binds package `33`, prints `ROBO_DRIVER: intel`, rolls the package back, and prints `ROBO_DRIVER: basic`. The AMD, NVIDIA, Realtek, Broadcom, and Qualcomm catalogs are files and tests only. The kernel does not call them.

```
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelCatalog.test.rx
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelGraphics.test.rx
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelAudio.test.rx
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelNet.test.rx
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelWiFi.test.rx
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelBluetooth.test.rx
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelLPSS.test.rx
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelUSB.test.rx
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelStorage.test.rx
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelCamera.test.rx
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelChipset.test.rx
robix test RoboticelaOS/Kernel/IO/Vendors/AMD/RoboAMDCatalog.test.rx
robix test RoboticelaOS/Kernel/IO/Vendors/NVIDIA/RoboNVIDIACatalog.test.rx
robix test RoboticelaOS/Kernel/IO/Vendors/Realtek/RoboRealtekCatalog.test.rx
robix test RoboticelaOS/Kernel/IO/Vendors/Broadcom/RoboBroadcomCatalog.test.rx
robix test RoboticelaOS/Kernel/IO/Vendors/Qualcomm/RoboQualcommCatalog.test.rx
robix test RoboticelaOS/Kernel/IO/RoboDriver.test.rx
```

Each printed `interpreter ok, native ok`.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -vga std -hda Build/RoboticelaOS/RoboDisk.img -serial file:Build/RoboticelaOS/qemu.serial
```

The four-core serial file, after the Stage Q class lines:

```
ROBO_DRIVER: basic
ROBO_DRIVER: storage
ROBO_DRIVER: display
ROBO_DRIVER: net
ROBO_DRIVER: intel
ROBO_DRIVER: basic
ROBO_FS: Note.txt
ROBO_SYSCALL: echo
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints `ROBO_CPU: cores online1` and the same `intel` then `basic` lines, and it does not print `ROBO_SCHED: migrate`.

## RoboticelaOS Stage AG

`GPU.Command` appends a fill or a blit to a four-slot software queue. `GPU.Submit` returns `-1` for a process other than the window server and leaves the queue unpainted. A window-server submit of a fill at `(1, 0)` and a blit of that pixel onto the origin returns `526864`, and `AssertPixel` reads that color at the origin and at `(1, 0)`. Two cores divide four rows into `[0, 2)` and `[2, 4)`, and every row belongs to one core.

`RoboBasicDisplay` runs that buffer on the CPU, one slice per online core. `IntelHardware(31)` and `NVIDIAHardware(22)` select command hardware. This QEMU display is the Bochs linear framebuffer, so the boot binds `RoboBasicDisplay` and does not print `ROBO_GPU: device`.

```
robix test RoboticelaOS/User/RoboWindowServer/RoboGPUQueue.test.rx
robix test RoboticelaOS/Kernel/IO/Basic/RoboBasicDisplay.test.rx
robix test RoboticelaOS/Kernel/IO/Intel/RoboIntelGraphics.test.rx
robix test RoboticelaOS/Kernel/IO/Vendors/NVIDIA/RoboNVIDIAGraphics.test.rx
```

Each printed `interpreter ok, native ok`.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -vga std -hda Build/RoboticelaOS/RoboDisk.img -serial file:Build/RoboticelaOS/qemu.serial
```

The four-core serial file keeps the desktop lines and adds the submit line after the driver rollback:

```
ROBO_GPU: present
ROBO_UI: click
ROBO_GPU: miss
ROBO_DRIVER: intel
ROBO_DRIVER: basic
ROBO_GPU: submit
ROBO_FS: Note.txt
ROBO_SYSCALL: echo
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints `ROBO_CPU: cores online1` and `ROBO_GPU: submit`, and it does not print `ROBO_SCHED: migrate`.

## RoboticelaOS Stage S

`Launch.Plan` orders services by the `after` field. Login waiting on Window returns `513`, which starts Window and then Login. A cycle returns `-1`. Before the volume is mounted, `System/Launch/RoboLaunch.rxb` is missing and the lookup returns `-1`. After the mount, that file is present and process 1 is RoboLaunch.

The window server maps the framebuffer with syscall `Map` and is the only caller whose `Draw` succeeds. A client without a window returns `-1`. The front window covers the one behind it, and an empty damage rectangle returns `-1`. The dark desktop is `526864` (`#080A10`) and the light desktop is `16119287` (`#F5F5F7`). Both themes use menu bar `948991` (`#0E7AFF`). The boot theme is dark. The desktop splash ceiling stays `99600`.

```
robix test RoboticelaOS/User/RoboLaunch/RoboLaunch.test.rx
robix test RoboticelaOS/User/RoboWindowServer/RoboWindowServer.test.rx
```

Each printed `interpreter ok, native ok`.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -vga std -hda Build/RoboticelaOS/RoboDisk.img -serial file:Build/RoboticelaOS/qemu.serial
```

The server sends `ready` on the launch port. The kernel then presents the server frame and stops painting. The four-core serial file, after the note:

```
ROBO_FS: Note.txt
ROBO_LAUNCH: pid 1
ROBO_WINDOW: ready
ROBO_KERNEL: console yield
ROBO_SYSCALL: echo
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints `ROBO_CPU: cores online1` and the same three launch lines, and it does not print `ROBO_SCHED: migrate`.

## RoboticelaOS Stage T

`Auth.Check` accepts password `4919` when the hash is `password xor 21072`, and it rejects a different password. A matching check opens an empty session, which has no user windows. `ACL.Allow` lets user 0 write `System/` and denies that write for user 1 and the guest. The guest can write `Home/Guest` and cannot write `Home/` or `System/`. Lock returns the login window when the two process address spaces differ, and it rejects one shared space.

The login window is window 1, the first window on the server desktop, at `16, 48` and `288` by `120`. A successful password starts Finder, then the Dock, then the menu extras. `Desktop/RoboLogin.rxu` has that window in the dark theme `#080A10` and the light theme `#F5F5F7`, both with menu bar `#0E7AFF`.

```
robix test RoboticelaOS/User/RoboLogin/RoboLogin.test.rx
```

It printed `interpreter ok, native ok`.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -vga std -hda Build/RoboticelaOS/RoboDisk.img -serial file:Build/RoboticelaOS/qemu.serial
```

The four-core serial file, after the console yield:

```
ROBO_KERNEL: console yield
ROBO_FS: deny
ROBO_SYSCALL: echo
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints `ROBO_CPU: cores online1` and `ROBO_FS: deny`, and it does not print `ROBO_SCHED: migrate`.

## RoboticelaOS Stage U

`UI.Stack` places three notifications of height 48 with gap 8 from origin 40 at y 40, 96, and 152. An index past the count returns -1. Dismiss of the middle note leaves two. `UI.Grid` places eight icons in four columns of 64: index 0 is `(0, 0)`, index 1 is `(64, 0)`, index 4 is `(0, 64)`, and index 7 is `(192, 64)`.

Finder is window 6. Space 1 is focused and space 0 is hidden. One drag command moves `(40, 96)` by `(12, -8)` to `(52, 88)`. Snap of that window on a 320 by 200 screen is the left half `0, 32, 160, 120`, the top-left quarter, and maximize `0, 32, 320, 120`. A different window id is rejected. `Clock.Seconds(89)` is 59. `Desktop/RoboDock.rxu` and `Desktop/RoboFinder.rxu` each have a dark screen `#080A10` and a light screen `#F5F5F7`, both with menu bar `#0E7AFF`. The Finder window is 288 wide on the 320-wide document and 640 wide on a 1920-wide display.

```
robix test RoboticelaOS/User/RoboDock/RoboDock.test.rx
robix test RoboticelaOS/User/RoboFinder/RoboFinder.test.rx
```

Each printed `interpreter ok, native ok`.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -vga std -hda Build/RoboticelaOS/RoboDisk.img -serial file:Build/RoboticelaOS/qemu.serial
```

The four-core serial file, after the console yield:

```
ROBO_KERNEL: console yield
ROBO_FS: deny
ROBO_SESSION: spaces
ROBO_SYSCALL: echo
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints `ROBO_CPU: cores online1` and `ROBO_SESSION: spaces`, and it does not print `ROBO_SCHED: migrate`.

## RoboticelaOS Stage V

`Settings.Get` returns the handoff display default, width `1920` and height `1080`, for the current user in `Home/`. `Settings.Set` stores a width of `1280`, and `Settings.Reset` returns `1920` again. Sound, network, Bluetooth, users, privacy, language, date, power, updates, and appearance each have a key. Appearance `0` is the dark desktop `#080A10` and appearance `1` is the light desktop `#F5F5F7`. The window server reads that appearance key when it paints the boot desktop.

`Search.Query` with the two-letter prefix `Ro` returns Files, Finder, then Settings. The prefix `No` returns `Home/Note.txt`. A later index returns `-1`.

```
robix test RoboticelaOS/User/RoboSettings/RoboSettings.test.rx
robix test RoboticelaOS/User/RoboSearch/RoboSearch.test.rx
```

Each printed `interpreter ok, native ok`.

```
qemu-system-x86_64 -cpu qemu64 -machine pc -smp 4 -kernel Build/RoboticelaOS/RoboKernel.elf -display none -vga std -hda Build/RoboticelaOS/RoboDisk.img -serial file:Build/RoboticelaOS/qemu.serial
```

The four-core serial file, after the console yield:

```
ROBO_KERNEL: console yield
ROBO_FS: deny
ROBO_SESSION: spaces
ROBO_SETTINGS: display
ROBO_SEARCH: hit
ROBO_SYSCALL: echo
ROBO_VM: fault
ROBO_PROCESS: alive
```

The same image with `-smp 1` prints `ROBO_CPU: cores online1`, `ROBO_SETTINGS: display`, and `ROBO_SEARCH: hit`, and it does not print `ROBO_SCHED: migrate`.

## How to update this file

Change a row only after a command was run. Record the command and the result under the row. Do not mark a feature Implemented because a specification section or an example file mentions it.
