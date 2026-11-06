# ROBIX — ENGINEERING PLAN
## A real systems language by Roboticela

**Tagline:** "The Language of Intelligent Machines."
**Owner:** Roboticela
**Implementation language:** C++20 / C++23
**Build system:** CMake, with the existing `Build/*.ps1` object build until CMake lists every compiler unit

This plan replaces the previous stage list. The product features below stay. A feature is complete only when it compiles, runs, and has a test that was actually executed. Documentation, a passing toy loop, or a namespace with no implementation does not count.

---

## 1. What this plan is for

Four outcomes, in this order:

1. **A language a programmer can use the way they use C.** One syntax, static types, native machine code, a small standard library, and diagnostics that name the file, line, and the fix.
2. **Honest performance.** Robix native code is compared with equivalent C and C++ on the same algorithm, the same optimization level, and the same machine. The published result includes the checksum and the compiler version. A win is claimed only when a cool re-run still wins.
3. **A freestanding target that can host a BIOS, a bootloader, a kernel, and an OS.** No hidden runtime, no required garbage collector, explicit `Unsafe`, architecture-checked inline assembly, and a linker story for a custom entry point.
4. **A language that stays easy to write.** Beginners use functions, values, and `Print`. Systems programmers opt into pointers, manual memory, and assembly. The same program means the same thing in the interpreter and in the native compiler.

RobixUI, RobixBuild, packages, robotics libraries, and Robix Network stay in the product. They are built after the language can compile the programs they depend on.

---

## 2. What is true today

Keep this section current. Do not copy it into release notes as a feature list.

| Area | State |
| :--- | :--- |
| Shared frontend (lexer, parser, types, SSA IR) | In the tree and used by `robixc` |
| Native backend | LLVM IR text, then `opt` and Clang. Procedural functions, integer and floating-point arithmetic, calls, `Print`, and `switch` lowering for equality chains |
| Native backend rejects | Classes, fields, virtual calls, heap and stack intrinsics, OS intrinsics, strings as values, `defer` |
| Interpreter | Bytecode VM exists. Semantic parity with native code is not proven for every feature |
| Benchmarks under `Benchmarks/` | Real timings of short synthetic kernels. File names such as SPEC, DaCapo, NPB, and Debian Benchmarks Game are labels, not those suites |
| Specification documents | Present under `Specification/`. They describe a larger language than the native compiler implements |
| RobixUI, RobixBuild, Network, LSP wire protocol, VS Code extension | Not the shipping path |

Release integer arithmetic is two's-complement wrap. `nsw` is not emitted on release add, sub, or mul. Horizontal SLP stays off. Vertical SLP stays on. Innermost loops are aligned to 32 bytes and branches are padded for this x86-64 target.

---

## 3. Product identity (kept)

### 3.1 Executables

| Tool | Role |
| :--- | :--- |
| `robixc` | Compiler. Development default is the interpreter only when the user asks to run. Production output is a native binary. A failed native compile is reported. It is never replaced by the interpreter. |
| `robix` | Package manager and project CLI |

### 3.2 Extensions

| Extension | Purpose |
| :--- | :--- |
| `.rbx` | Source |
| `.test.rbx` | Tests for the matching source file |
| `.rbxh` | Interface / header |
| `.rbxu` | RobixUI document |
| `.rbxtheme` | UI theme |
| `.rbxassets` | UI asset manifest |
| `.rbxb` | RobixBuild project file |
| `.rbxproj` | Package manifest |
| `.rbxapi` | Public API with `Explain` blocks and no implementation |
| `.rbxp` | Binary package archive |
| `.rbxlock` | Lockfile |
| `.rbxm` | Compiled module |
| `.rbxlib` | Library |
| `.rbxworkspace` | Workspace |
| `.env`, `.env.client`, `.env.server` | Scoped configuration for applications that opt into Robix Network |

Do not add another extension for the same job.

### 3.3 Names

Enforced by `robixc`, `robix format`, and `robix lint`. Legacy projects may opt out in the project file. The compiler does not rename identifiers.

- **PascalCase:** files, packages, types, functions, methods, constructors, enum names.
- **camelCase:** locals, fields, parameters.
- **UPPER_SNAKE_CASE:** constants.
- **Acronyms stay uppercase:** `HTTPClient`, `GPIOPin`, `apiURL`. `HttpClient` is rejected in strict mode.

### 3.4 Language shape (kept)

- Primary style is object-oriented. Top-level functions are legal. A program does not need a class.
- Functional forms that must exist once the core compiles: lambdas, closures, `Map` / `Filter` / `Reduce`, `match`, immutable `let`.
- Types: `Bool`, `Byte`, `Char`, `Int8`–`Int64`, `UInt8`–`UInt64`, `Float32`, `Float64`, `Int`, `UInt`, `Float`, `Double`, `String`, `Void`. Sizes and alignment are in the specification and are not platform-ambiguous.
- Also required, after the core: arrays, slices, tuples, structs, enums, classes, interfaces, generics, `Option<T>`, `Result<T, E>`, function types, pointers, references.
- Memory: automatic objects and manual `Unsafe` allocation in one program. The runtime must not free memory the programmer owns. The programmer must not free memory the runtime owns unless ownership was transferred.
- `Unsafe` is the only place for raw pointer dereference, pointer arithmetic, and inline `ASM`.
- Inline assembly is checked against the selected target: `x86_64`, `ARM64`, `ARM32`, `RISC-V`.
- Errors: `Result` for expected failure, exceptions for exceptional failure. Robotic and kernel faults are reported. They are not ignored.
- Concurrency, after the core: threads, `Task<T>`, `async` / `await`, `Mutex`, `Atomic<T>`, channels. A program that uses none of these does not link their runtime.
- `Explain { }` blocks on public API.
- One `Main` entry. `Main() -> Int` is the process exit code. Freestanding targets use the entry symbol from the `.rbxb` file instead of a hosted `main`.

### 3.5 Execution

```text
source -> lexer -> parser -> AST -> semantics -> SSA IR
        -> interpreter bytecode
        -> LLVM IR -> opt -> clang -> native binary
```

One frontend. Interpreter and native compiler share name resolution, types, and diagnostics.

| Command | Meaning |
| :--- | :--- |
| `robix run File.rbx` | Interpreter. Fast startup. |
| `robix run File.rbx --engine native` | Compile, then run the binary. |
| `robixc File.rbx -O3 -o App` | Native release build. |
| `robix build --profile debug\|release\|performance\|realtime\|embedded\|safety\|size` | Profile from the project file. |
| `robix repl` | Official grammar and type checker. |

Profiles:

| Profile | Contract |
| :--- | :--- |
| `debug` | Checks, symbols, little optimization |
| `release` | Optimize. Integer wrap. Fast floating point that is documented |
| `performance` | Release plus the extra LLVM passes that benchmarks show are profitable |
| `realtime` | No unbounded allocation on the hot path. No claim of hard real time unless the OS provides it |
| `embedded` / `size` | Freestanding, small binary |
| `safety` | Extra static and runtime checks |

### 3.6 Repository layout (kept)

`Compiler/`, `Interpreter/`, `Runtime/`, `RobixUI/`, `RobixBuild/`, `Network/`, `Packages/`, `Tools/`, `StdLib/`, `Specification/`, `Tests/`, `Examples/`, `Documentation/`, `Benchmarks/`.

### 3.7 Project modes (kept)

1. **Single file.** `robix run Hello.rbx` with no project file.
2. **Application.** Games, desktop, CLI, libraries, firmware, kernels. No network runtime unless the project asks for it.
3. **Network.** Optional. `Common/` contracts, clients, and servers. A normal program does not link it.

Official styles, used by `robix new` once that command exists: `SingleFile`, `Application`, `Game`, `Library`, `Embedded`, `System`, `Network`, `Custom`.

`.rbxb` output kinds that must exist before the OS stage is called done: `Executable`, `Library`, `SharedLibrary`, `StaticLibrary`, `Object`, `Firmware`, `Kernel`, `Package`.

---

## 4. Engineering rules (kept, and applied to this plan)

1. Never implement fake functionality.
2. Never mark a stage complete because a header or a document exists.
3. Never report a test as passed unless it was executed.
4. Never claim hardware works on a board that was not run.
5. Never publish a benchmark whose Robix and C sources do different amounts of work.
6. Never name a local kernel after SPEC, DaCapo, NPB, or the Benchmarks Game unless it is that suite, unmodified, and cited.
7. Never change a benchmark to make Robix win. Change the compiler, then re-measure.
8. Never treat a hot full-suite run on a laptop as the result. Re-time a loser cool, interleaved with the C and C++ binaries, and publish that.
9. Safe code has defined overflow (wrap in release). `Unsafe` is explicit.
10. Diagnostics carry a code, a location, and a suggestion.
11. Language changes that break source require a `LanguageVersion` bump and a changelog entry.
12. The C++ compiler itself uses RAII, const, and sanitizers on tests. Robix's memory model is separate from the compiler's allocations.

---

## 5. The new stages

Work top to bottom. A later stage may have a prototype. It is not a deliverable until its exit test passes.

### Stage A — Honest baseline

**Status: implemented.**

- `Documentation/Status.md` lists Implemented, Partial, and Unsupported against what `robixc` accepts. Classes, a garbage collector, GPIO, and packages are Unsupported on the native backend.
- `Benchmarks/README.md` and a README in each suite folder state that the kernels are local and synthetic.
- `Benchmarks/RunBenchmarks.ps1` prints `robixc --version`, Clang's version line, the exact flags, each program's checksum line, and an interleaved average. The banner says these runs are not SPEC CPU, DaCapo, NPB, or the Benchmarks Game.

**Exit test:** those files exist and the runner banner does not call a local folder an upstream suite.

### Stage B — A C-class core that compiles

This is the language programmers write before they need objects.

**Must compile and run, interpreter and native, with the same result:**

- Functions, overloads, `Int` / `Float` / `Bool` / fixed-width integers.
- `let`, `var`, `const`, explicit types.
- `if`, `while`, `for`, `break`, `continue`, `return`.
- Arrays and slices with bounds checks in safe profiles.
- Structs, enums, tuples.
- `String` as UTF-8, including `Print` and concatenation, without a hidden per-call heap when the optimizer can see a literal.
- `Result` and `Option`.
- Pointers and `Unsafe` blocks: address-of, dereference, arithmetic, `Memory.Allocate` / `Memory.Free`.
- Power-of-two division and remainder lowered to shifts and masks when the value is in range, and to a correct signed sequence when it is not.
- C ABI calls (`extern` functions) so Robix can call a C library and can be called from C.

**Exit test:** a freestanding-free "libc-shaped" sample (functions, structs, arrays, strings, pointers, one C call) builds with `robixc -O3`, prints the same checksum in the interpreter, and links to a native executable.

Passed. `Examples/StageB/LibcShape.rbx` prints `ok`, `Hello`, and checksum `506` from `robix run` and from `robixc -O3` linked to `Build/StageB.exe`. Calling C (`extern puts`) works. A C program cannot call a Robix function yet: Robix functions stay `fastcc` and mangled. Inbound C exports belong with the freestanding work in Stage H.

### Stage C — Objects, when the programmer asks for them

**Must compile natively, not only in the interpreter:**

- Classes, constructors, methods, fields, `private` / `public` / `protected` / `internal` / `package`.
- Interfaces, virtual calls, `override`.
- Generics monomorphized in the native backend.
- Deterministic destruction for values with a destructor. Reference-counted objects only where the type says so. A leaf function that does not allocate does not pull in a collector.

**Exit test:** the motor-controller sample (a class, a private field, a method call) is a native binary, and a virtual call sample matches the interpreter's output.

Passed. `Examples/StageC/Machine.rbx` prints `150` and then `40` from `robix run` and from `robixc -O3` linked to `Build/StageC.exe`. The `40` is `Motor`'s destructor. `new` that the compiler can see is a direct call. `MakeRunner` returns a `Runner` through `Walker`, and that call is virtual in both engines. `Box<Int>` is a separate native instantiation. `Leaf` does not allocate. Reading `Motor.speed` from `Main` in `Examples/StageC/PrivateField.rbx` is `R2008`. There is no collector and no reference-counting runtime. A class with more than one interface does not get a correct combined vtable. Inbound C calls are still Stage H.

### Stage D — Programmer-friendly by default

The core is useless if only the author of the compiler can satisfy it.

- One recommended syntax in the tutorial: `Function Name(Int x) -> Int`. The C-style declaration form stays legal and means the same thing.
- Errors name the expected type and the received type.
- `Print` and `Input` work in hosted programs.
- A standard library that is real code: `Array`, `List`, `Math`, `String` methods, `File` on hosted targets. Empty namespaces are Unsupported, not shipped.
- `robix new` writes a single-file program that runs.
- The first ten pages of the language guide match the compiler. Where the guide is ahead of the compiler, the guide says so in the first paragraph.

**Exit test:** a person who has not read the compiler can compile the guide's hello-world, struct, loop, and error-handling samples with the documented commands.

Passed. `Documentation/Guide.md` is the command list. These were run on `robix run` and `robixc -O3`:

| Sample | Prints |
| :--- | :--- |
| `Examples/StageD/Hello.rbx` | `Hello, Robix`, then `42` |
| `Examples/StageD/Point.rbx` | `7` |
| `Examples/StageD/Loop.rbx` | `10` |
| `Examples/StageD/Divide.rbx` | `0`, then `5` |
| `StdLib/Hosted.rbx` | `45`, `hosted`, `4`, `10`, `9` |

`Examples/StageD/Ask.rbx` with the lines `Ada` and `41` prints `Ada` and `42` on both engines. `Examples/StageD/Mismatch.rbx` is `R1001` with `Expected: Int` and `Received: String`. `robix new HelloApp` writes `HelloApp/Main.rbx`, and that file prints `Hello from Robix` from both engines. `Int Name(Int x)` without an arrow is the C-style form and is what `Hello.rbx` uses. `Function Name(Int x) -> Int` is the recommended form. `robix run` keeps `File` in memory. The native binary writes the host file `Build/StageDHosted.txt`. There is no `import` of one `.rbx` file from another. `match` and `try` are not compiled.

### Stage E — Genuine benchmarks

Replace the scoreboard as the source of truth.

Each published comparison has:

- Robix, C, and C++ sources that implement the same algorithm, the same trip count, and the same checksum.
- Flags recorded: `robixc -O3` and `clang -O3 -march=native` (or the freestanding flags, when that is the target).
- Five measured runs after two warmups, highest run dropped, plus one cool interleaved confirmation for any claimed win or loss.
- No thermal result from a 60-binary sweep used as the only evidence.

Suites, in order:

1. **Micro.** Arithmetic, branches, calls, integer division, Collatz-style loops, allocation, string scan. These may live in `Benchmarks/Micro/`.
2. **Numeric.** Matrix multiply, FFT-sized loop, particle step, PID. Equivalent C beside each file.
3. **Systems.** Boot-path integer code, memcpy, a tiny interrupt-safe ring buffer. No OS calls in the timed region.
4. **External suites, only if obtained and run.** Real SPEC, real NPB, or real Benchmarks Game ports are a separate directory, with the upstream version in the README. Until that tarball is in the tree and the run is scripted, those names do not appear in the summary.

**Exit test:** the micro suite's README lists wins, ties, and losses. A loss stays a loss. The script refuses to print "SPEC" or "DaCapo" unless that suite's manifest says `upstream: true`.

Passed for the local suites. `Benchmarks/Micro/README.md` is the score from `RunBenchmarks.ps1 -Suite All -Runs 5 -CoolSeconds 15` with `robixc -O3` and `clang` / `clang++ -O3 -march=native` (Clang 18.1.8). Five rounds, starting language rotated, highest dropped, then one cool sample. Within 5 percent of the fastest five-run time is a tie, and one cool sample does not overturn that tie. A win or a loss is published only when the cool sample agrees on direction.

| Published | Kernels |
| :--- | :--- |
| Tie | Allocate, Arithmetic, Branches, Collatz, Division, Scan, FftLoop, Matrix, Particle, Pid, BootPath, Ring |
| Unconfirmed | MemCopy (14.92 ms vs C 14.10 ms; the cool sample tied, and a repeat of the same binaries was inside 5 percent) |
| Suite win, not stable | Calls. The suite printed 16.93 ms vs C 22.75 ms. A repeat of those binaries was 9.53, 9.95, and 10.05 ms |

Score as printed: 1 win, 12 ties, 0 losses, 1 unconfirmed. The win does not survive a repeat. `n % 2 == 0` is a bit test, and an even division by 2 is an arithmetic shift. No manifest in this tree sets `upstream: true`. The script exits 2 on any other suite name and does not print that name.

### Stage F — Freestanding: BIOS, bootloader, kernel, OS

This stage is what makes systems programming a real target. It starts as soon as Stage B can emit a C ABI object. It is not blocked on RobixUI or the network stack.

**Compiler and runtime:**

- `--target` triples: `x86_64-pc-windows-msvc`, `x86_64-unknown-linux-gnu`, `x86_64-unknown-none`, `aarch64-unknown-none`, `riscv64-unknown-none`.
- `Profile = embedded` produces an object or a linked image with no hosted CRT, no `Print` pulled in unless the program calls it, and a user-supplied entry symbol.
- Integer wrap, no collector, no thread runtime unless referenced.
- Volatile and address-space access for memory-mapped registers, specified and tested with a fake device region in a hosted test and, separately, noted as not a board test.
- Inline `ASM` with inputs, outputs, and clobbers, rejected when the instruction does not exist on the target.
- Linker script or `lld` flags selected by the `.rbxb` `Kernel` / `Firmware` output.
- Panic and abort that do not allocate.

**Samples that must link, not merely parse:**

| Sample | Proves |
| :--- | :--- |
| Multiboot or custom-entry stub that writes a serial byte | Bootloader-shaped entry, no CRT |
| Page-table builder in Robix plus a C or assembly trampoline | Pointers, integers, freestanding |
| Round-robin thread switch over a saved stack | The kernel primitive, still without claiming a full OS |
| Timer interrupt handler marked `Unsafe` and non-allocating | Interrupt rules |

A full BIOS image, a full kernel, and a full OS are programs written in Robix after this stage. They are not features of the compiler. The compiler's job is to make those programs possible: freestanding codegen, a stable ABI, and samples that boot in QEMU.

**Exit test:** `robixc --target x86_64-unknown-none -O3` links the serial stub, QEMU prints the expected byte, and the hosted checksum test of the same pure functions still matches C. The status file says "QEMU" or "hardware" next to each systems sample. A sample that only ran on Windows is not marked as a BIOS.

Passed. `robixc --target x86_64-unknown-none -O3 Examples/StageF/Serial.rbx -o Build/StageF/serial.elf` linked the multiboot image. QEMU wrote serial byte 82 (`R`). `Examples/StageF/Checksum.rbx` and `Examples/StageF/Checksum.c` both printed `8503` on Windows. `Start` writes `R` only when that checksum is 8503. `Volatile.rbx` printed 65 on Windows and is not a board test. `BadMnemonic.rbx` is `N9010` on `aarch64-unknown-none`. A plain `--target` for AArch64, ARM, RISC-V, and Linux still produces a relocatable object. A Kernel project for those three boards links a QEMU image: `Examples/StageF/Board.rbx` (`Console.Write(82); Cpu.Halt();`) wrote serial byte 82 on `qemu-system-x86_64`, `qemu-system-aarch64 -machine virt`, `qemu-system-arm -machine virt`, and `qemu-system-riscv64 -machine virt -bios none`. Linux was not booted. `Documentation/Status.md` lists QEMU or Windows on each row.

### Stage G — One semantics, two engines

- Bytecode version checked before execution.
- Conformance tests: arithmetic, wrap, branches, calls, arrays, structs, pointers where the interpreter can represent them, and an explicit diagnostic where it cannot.
- A native failure is not hidden by running the interpreter.
- `robix test` runs `.test.rbx` on the interpreter and, for tests marked native, on the compiled binary.

**Exit test:** the conformance directory is green on both engines, and the list of "native only" operations is short and written down.

Passed. `robix test Tests/Conformance` printed `conformance: both engines passed`. The interpreter and the native executables agreed on arithmetic, wrap (`1 << 62` added to itself and multiplied by 4), branches, calls, arrays, structs, and pointers. A module with bytecode version 99 was refused before any instruction ran. `NativeOnly.test.rbx` stopped in the interpreter on `Port.Write8`. `Tests/Conformance/NATIVE_ONLY.md` lists the operations the interpreter refuses. `Assert.Equal` and `Assert.True` compile. A native failure is reported even when the interpreter passed.

### Stage H — Libraries a programmer actually calls

Build these as Robix source under `StdLib/`, each with `.test.rbx`, each compiling natively:

- `Robix.Core`, `Robix.Collections`, `Robix.Math`, `Robix.String`, `Robix.IO` (hosted).
- `Robix.Memory` (allocators, arenas, the manual API).
- `Robix.Threading` and `Robix.Atomics`.
- `Robix.Hardware` with a portable trait and a target-specific body. A GPIO call on Windows returns a clear "not this target" error. It does not return a fake pin value.
- `Robix.Robotics` algorithms that are pure (PID, transforms, quaternions) separated from device drivers.
- JSON and binary encoding after strings and slices exist.

AI, GPU, and database packages stay later. They are real dependencies with a test, or they are absent.

Passed. `robix test StdLib/Robix` printed `conformance: both engines passed`. The Robix sources under `StdLib/Robix/` cover Core, Collections, Math, String, hosted IO, a manual byte buffer, `Atomic.FetchAdd`, `Thread.AddPair`, `GPIO.Read`, integer PID, quarter-turn rotation, a quaternion product, JSON `{"n":42}`, and a two-byte pack. Native `Thread.AddPair` starts two Windows threads and joins them. The interpreter performs those two additions on the calling thread. `GPIO.Read` on Windows returns 1 and writes `GPIO.Read is not available on this target (pin 13)`. The hardware test checks that error. It does not treat 1 as a pin level. `Build/StageH-io.txt` contains `RobixIO`.

### Stage I — Packages, tests, and RobixBuild

Kept from the previous plan, implemented only on top of Stage B and Stage H.

- `.rbxproj`, `.rbxlock`, `.rbxapi`, `.rbxp`, signatures, SHA-256.
- `robix add`, `remove`, `update`, `install`, `restore`.
- Per-file `.test.rbx`. `robix test` fails the build when a required test file is missing, once the project opts in. A brand-new single file can run without a test file.
- `.rbxb`: sources, `Order`, output kind, targets, configs, `--explain`, `--dry-run`.
- Incremental build that rebuilds a file when that file or a file it imports changes.

Passed. `robix add Robix.Answer Examples/StageI/Vendor/Answer.rbx --project Examples/StageI` wrote `StageI.rbxlock` with SHA-256 `d6a6c0a8516b9c10ef7dbb24e3d7245f66a1123ea4187ecb7d078af7f865a070` and an HMAC-SHA256 signature from the project `SignKey`. Appending a byte made `robix restore` exit 1 with `hash or signature does not match the lock`. `robix update` rehashed the restored file, `robix install` copied it to `Packages/Robix.Answer/` with `Robix.Answer.rbxapi` and `Robix.Answer.rbxp`, and `robix remove` deleted the lock entry. There is no network registry. `RequireTests: true` made `robix test` and `robix build` exit 1 while `Source/Gap.rbx` had no `Gap.test.rbx`. After that source was removed, `robix test Examples/StageI` printed `conformance: both engines passed`. `robix build Build/StageI-lone/Lone.rbx` compiled a file that has no test and no project, and `Lone.exe` printed `1`. `robixc Examples/StageI/App.rbxb --explain` and `--dry-run` printed `order Source/Add.rbx Source/Main.rbx` and `dry-run: compile was not started`. `Examples/StageF/Kernel.rbxb` still explains as a kernel whose entry is `Kernel.rbx`. The second `robix build` of `Source/Main.rbx` printed `incremental: unchanged`. Changing imported `Add.rbx` rebuilt `Build/StageI/stagei.exe`, which printed `42`.

### Stage J — RobixUI

Kept. Starts when Stage C can call a method and Stage D can name a function from a document.

- `.rbxu` parser, component tree, layout, style, events.
- Bind `OnClick` to a Robix method with a type check.
- One software renderer or one OS backend, named in the status file. No backend is described as cross-platform until that OS has been run.

Passed. `robix ui validate Examples/StageJ/MainWindow.rbxu --code Examples/StageJ/Main.rbx` parsed the document, laid out the window, column, row, text, and buttons, and bound `OnClick` to `MainController.StartRobot() -> Void` and `MainController.StopRobot() -> Void`. `Examples/StageJ/BadWindow.rbxu` exited 1: `SetSpeed` has an `Int` parameter, and `Missing` is not a function. `robix ui render` wrote `Build/StageJ/light.bmp`. The file's pixel 0,0 is `F4F5F7` and pixel 18,44 inside Start is `1D4E89`. `--theme dark` wrote `Build/StageJ/dark.bmp` with 0,0 as `14161A` and the button pixel as `4C8DFF`. `--width 200` wrapped `StopButton` to y 80, and pixel 18,82 in `Build/StageJ/narrow.bmp` is `1D4E89`. The renderer is a software BMP writer run on Windows. It does not open a window and it is not a cross-platform backend. The click binding is the type check. The renderer does not call the method.

### Stage K — Robix Network

Kept, and optional. A project with no `Network` block does not compile or link it.

- `Common/` contracts, typed calls, events, `.env` split by client and server, TLS, timeouts.
- Secrets do not ship inside client binaries.

Passed. `robix network run Examples/StageK` parsed `Common/Status.rbx` as `GetStatus(Int) -> Int` and `BatteryLow(Int) -> Void`. The call on `127.0.0.1` returned `reply Int 42`, the event was `BatteryLow Int 7`, and `call GetStatus String fast` returned `error expected Int`. The hang connection timed out after 203 ms. Schannel completed a handshake with the local self-signed certificate whose subject is `robix-stagek`, and the typed call inside that session returned `reply Int 42`. This is not a public certificate authority and not an internet connection. `Examples/StageK-bad` exited 1 because `SERVER_TOKEN` is in both `.env.client` and `.env.server`. `Build/StageK/client.env` contains `APP_NAME` and `PUBLIC_HOST` only. `robix build Build/StageK-plain/Plain.rbx` printed `network-runtime: omitted`, and `Plain.exe` does not contain `rbx-network-runtime-marker`. `robix build Examples/StageK/Source/Client.rbx` printed `network-runtime: linked Build/obj/NetworkRuntime.obj`. `client.exe` contains that marker and does not contain `STAGEK_SERVER_TOKEN_9f3a` or `STAGEK_CERT_PASSWORD_c4e1`. The same run then exchanged UDP snapshots for `Move(Int, Int, Int, Int) -> Void`: two clients, 39 snapshots applied in 297 ms, player 1 at 60,40 and player 2 at 120,20. An older sequence was ignored and a non-integer snapshot was rejected. The packets stayed on `127.0.0.1`.

### Stage L — Tools

- `robix format`, `robix lint`, `robix check`.
- Debugger that steps native code and the interpreter with the same source lines.
- LSP over the real protocol, then a VS Code extension for `.rbx`, `.rbxu`, and `.rbxb`.
- `robix doc` from `Explain` blocks.

Passed. `robix format Examples/StageL/Messy.rbx --check` exited 1, then `robix format` rewrote the `Print` line with 4 spaces, and a second `--check` printed `format: already formatted`. `robix lint Examples/StageL/Bad.rbx --strict` exited 1 with `L0004` for `maxSpeed` and `D3001` because `Bad` has no `Explain` block. `robix lint Examples/StageL/Clean.rbx --strict` and `robix check` on that file exited 0. `robix check Examples/StageL/TypeError.rbx` exited 1 with `R1001`: `String` assigned to `Int`. `robix doc` wrote `Build/StageL/Clean.md` with Description, Input, Processing, and Output for `Add`, and the same fields for the nested method `Counter.Value`. `robix debug Examples/StageL/Debug.rbx` printed `interpreter: 4 5 3` and `native: 4 5 3`. `Build/StageL/debug.ll` contains `call void @rbx_debug_line`. `robix build` of the same file with `--keep-ir` wrote `Build/StageL/plain.ll`, which has no `rbx_debug_line` call. `Examples/StageL/LspClient.ps1` spoke Content-Length JSON-RPC: `initialize`, `textDocument/didOpen`, `textDocument/hover` on `Value`, and `shutdown`. The hover text was `Read the counter.` with Processing `Return zero.` and Output `Zero.`. `Editors/vscode` registers `.rbx`, `.rbxu`, and `.rbxb` and starts `robix lsp`. That extension was not installed in VS Code.

---

## 6. Performance rules

Robix native code should match optimized C on the same algorithm. That is a target, not a claim.

- Release codegen stays wrap-correct. Do not turn signed remainder into `and` unless the dividend is non-negative or the identity holds for every two's-complement value.
- Measure before adding an LLVM flag. Horizontal SLP, extra unroll, and loop alignment are kept only while the cool micro suite does not regress the kernels that were already ahead.
- Publish losses. A 2% loss on a hot laptop is reported as noise. A 10% loss on a cool interleaved run is a compiler bug until the assembly shows the sources differ.

---

## 7. Order of work

```text
A Honest baseline
B C-class core (native + interpreter)
C Objects on the native backend
D Guide, errors, first standard library
E Genuine benchmarks          \
F Freestanding / QEMU boot    /  B unblocks both; F does not wait for UI
G Engine parity
H Standard library and hardware traits
I Packages, tests, RobixBuild
J RobixUI
K Robix Network
L Formatter, linter, debugger, LSP
```

Stages E and F are the next engineering work after B. Stages J and K are not reasons to delay a kernel object file.

---

## 8. Readiness

Boxes are empty until the exit test of that stage has been run and the command is recorded in `Documentation/Status.md`.

### Language
- [ ] B. Functions, structs, arrays, strings, `Result`, pointers, `Unsafe`, C ABI
- [ ] C. Classes, interfaces, virtual calls, monomorphized generics, destructors
- [ ] G. Interpreter and native conformance suite

### Systems
- [x] F. `x86_64-unknown-none` object with a custom entry
- [x] F. QEMU serial stub written in Robix
- [x] F. Inline assembly checked per target
- [x] F. Memory-mapped register sample with a hosted mock and a separate QEMU note

### Honesty and performance
- [x] A. Benchmark names that match what the files are (`Benchmarks/README.md`, suite READMEs, runner banner)
- [ ] E. Micro suite with matching C and C++ and published checksums
- [ ] E. No SPEC / DaCapo / NPB label unless the upstream suite is what ran

### Programmer experience
- [ ] D. Guide samples that compile with the documented command
- [ ] D. Diagnostics with type expected, type found, and a suggestion
- [ ] H. Core, collections, math, memory, and hosted IO tested
- [x] L. Formatter, linter, LSP, debugger

### Product features kept from the previous plan
- [ ] I. `.rbxproj`, lockfile, `.rbxapi`, `.rbxp`, `robix add`
- [x] I. `.rbxb` outputs including `Firmware` and `Kernel`
- [ ] I. Per-file tests
- [x] J. `.rbxu` parser, layout, type-checked events, one real renderer
- [x] K. Optional network contracts, env split, no client secrets
- [ ] H. Hardware traits that fail clearly off-target
- [ ] H. Pure robotics algorithms (PID, transforms) separate from drivers

---

## 9. Definition of done for the four goals

| Goal | Done when |
| :--- | :--- |
| Good language, in the sense of C | Stage B and Stage C exit tests pass, and a 500-line program of functions, structs, and one class builds native with no compiler change |
| Genuine benchmarks | Stage E script is the only scoreboard, and every row names real source files |
| BIOS, bootloader, kernel, OS | Stage F QEMU stub boots, the freestanding profile links without the hosted runtime, and the status file does not call a Windows `.exe` a BIOS |
| Easy to write | Stage D guide samples compile unchanged, and a hosted `Main` that only calls `Print` fits on one screen |
