# ROBIX — ENGINEERING PLAN
## A real systems language by Roboticela

**Tagline:** "The Language of Intelligent Machines."
**Owner:** Roboticela
**Implementation language:** C++20 / C++23
**Build system:** CMake, with the existing `Build/*.ps1` object build until CMake lists every compiler unit

A feature is complete only when it compiles, runs, and has a test that was actually executed. Documentation, a passing toy loop, or a namespace with no implementation does not count. Stages A through L are recorded in `Documentation/Status.md`.

---

## 1. What this plan is for

Four outcomes, in this order:

1. **A language a programmer can use the way they use C.** One syntax, static types, native machine code, a small standard library, and diagnostics that name the file, line, and the fix.
2. **Honest performance.** Robix native code is compared with equivalent C and C++ on the same algorithm, the same optimization level, and the same machine. The published result includes the checksum and the compiler version. A win is claimed only when a cool re-run still wins.
3. **A freestanding target that can host a BIOS, a bootloader, a kernel, and an OS.** No hidden runtime, no required garbage collector, explicit `Unsafe`, architecture-checked inline assembly, and a linker story for a custom entry point.
4. **A language that stays easy to write.** Beginners use functions, values, and `Print`. Systems programmers opt into pointers, manual memory, and assembly. The same program means the same thing in the interpreter and in the native compiler.

---

## 2. What is true today

`Documentation/Status.md` is the record of what has been run. Do not copy it into release notes as a feature list.

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

Official kinds for `robix new`: `Blank`, `CLI`, and `Network`. `.` writes into the current directory. A missing name or kind is asked, and a folder name that the operating system cannot use is asked again. `robix new file` adds a `hello`, `empty`, `class`, or `package` source file. A Network project keeps each client and server folder in the project root. Each one is `CLI` or `GUI`. A GUI service also gets a `.rbxu` window. `Common` has `Source` and `Tests`, and those files only declare the calls. `.env.common` and `exp.env.common` hold the shared secret, hosts, and ports. The root has no lone `.env`. Every other env file is empty. A new project includes `.gitignore`. `import Robix.Env` provides `Get`, `Exists`, `Set`, `Add`, and `Remove`. `Get` warns on its own when the name is missing or empty. `robix run all` runs the services in the current console. `--separate` gives each service its own console. The project `.rbxb` lists the build order, with `Common` before the programs that import it, and lists the UI files.

`.rbxb` output kinds `Executable`, `Library`, `SharedLibrary`, `StaticLibrary`, `Object`, and `Package` are recorded in `Documentation/Status.md`. `Firmware` and `Kernel` already link.

---

## 4. Engineering rules

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

## 5. Still open

- An upstream benchmark suite whose manifest says `upstream: true`. The published score stays the local micro suite. Do not set `upstream: true` on a local kernel, and do not name one after SPEC, DaCapo, NPB, or the Benchmarks Game.

---

## 6. Performance rules

Robix native code should match optimized C on the same algorithm. That is a target, not a claim.

- Release codegen stays wrap-correct. Do not turn signed remainder into `and` unless the dividend is non-negative or the identity holds for every two's-complement value.
- Measure before adding an LLVM flag. Horizontal SLP, extra unroll, and loop alignment are kept only while the cool micro suite does not regress the kernels that were already ahead.
- Publish losses. A 2% loss on a hot laptop is reported as noise. A 10% loss on a cool interleaved run is a compiler bug until the assembly shows the sources differ.
