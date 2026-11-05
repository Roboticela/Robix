# ROBIX PROGRAMMING LANGUAGE & ROBIXUI — MASTER ENGINEERING PLAN
## A Complete Programming Language Ecosystem by Roboticela

**Tagline:** *"The Language of Intelligent Machines."*  
**Official Philosophy:** *High performance. Complete control. Intelligent machines. Write simply. Type safely. Test every file. Run concurrently. Compile natively. Control everything.*  
**Project Owner:** Roboticela  
**Implementation Language:** Modern C++ (C++20 / C++23)  
**Primary Build System:** CMake  

---

## SECTION 1: PRODUCT IDENTITY, EXTENSIONS & CONVENTIONS

### 1.1 Product Identity & Executables
- **Primary Language:** Robix
- **Declarative UI Language:** RobixUI (XML-inspired, natively integrated with Robix)
- **Declarative Build Configuration Language:** RobixBuild (`.rbxb` — statically typed, declarative build & project architecture language)
- **Distributed Application Framework:** Robix Network (optional built-in multi-client, multi-server & shared contract architecture)
- **Compiler Executable:** `robixc`
- **Package Manager & Unified CLI Executable:** `robix`
- **Official Company:** Roboticela

### 1.2 Official File Extensions
Every file extension in the Robix ecosystem has a strictly defined purpose. Do not introduce unnecessary or alternative extensions:

| Extension | Official Purpose |
| :--- | :--- |
| `.rbx` | Robix source code |
| `.test.rbx` | Mandatory source-file tests corresponding to a `.rbx` file |
| `.rbxu` | RobixUI declarative user interface markup (strictly `.rbxu`) |
| `.rbxb` | **RobixBuild** declarative, statically typed build configuration file (`Project.rbxb`) |
| `.rbxproj` | Project metadata & dependency manifest (`Project.rbxproj`) |
| `.rbxapi` | Public package API and interface definition (declarations + `Explain` blocks, no implementation) |
| `.rbxp` | Compiled Robix binary package archive (distributable bundle) |
| `.rbxlock` | Locked dependency versions, hashes, and artifact metadata (`Robix.Lock` / `robix.lock`) |
| `.env` / `.env.client` / `.env.server` | Scoped environment configuration files for applications, clients, and servers |

### 1.3 Official Naming & Capitalization Conventions
Naming consistency is enforced by the compiler (`robixc`), formatter (`robix format` / `robix fmt`), and linter (`robix lint`, `robix lint --strict`). Legacy projects may use explicit configuration overrides, but official defaults strictly enforce:

1. **PascalCase**
   - **Used for:** Filenames (`RobotController.rbx`, `MotorDriver.rbx`, `NeuralNetwork.rbx`, `HardwareManager.rbx`), Class names, Interface names, Struct names, Enum names & variants, Function names, Method names, Constructor names, Public type names, Package names and package segments, Traits, Type aliases.
2. **camelCase**
   - **Used for:** Local variables (`motorSpeed`, `robotName`, `isConnected`), Instance variables, Parameters, Function arguments, Properties (where appropriate to local/instance rules or per explicit class member specs), Non-constant fields.
3. **UPPER_SNAKE_CASE (CAPITAL_CASE)**
   - **Used for:** All compile-time constants (`MAX_SPEED`, `PI_VALUE`, `COMPANY_NAME`).
   - Strict style mode in the compiler rejects improperly named constants.
4. **Acronym & Abbreviation Capitalization Policy**
   - Acronyms and abbreviations **must remain uppercase** (`SDK`, `XML`, `CPP`, `API`, `HTTP`, `GPU`, `CPU`, `USB`, `AI`, `JSON`, `TCP`, `UART`, `PWM`, `GPIO`, `I2C`, `SPI`, `CAN`, `DMA`).
   - Never convert an acronym into ordinary PascalCase or camelCase.
   - **Correct PascalCase Examples:** `APIManager`, `HTTPClient`, `GPUController`, `CPUInfo`, `USBDevice`, `AIModel`, `XMLParser`, `JSONReader`, `TCPConnection`, `UARTDriver`.
   - **Incorrect Examples:** `ApiManager`, `HttpClient`, `GpuController`, `CpuInfo`, `UsbDevice`, `AiModel`, `XmlParser`, `JsonReader`, `TcpConnection`, `UartDriver`.
   - **Correct camelCase Variable Examples:** `apiURL`, `httpResponse`, `gpuTemperature`, `isUSBConnected`.
   - **Policy:** Never silently rename user identifiers; reject or warn via diagnostics.

---

## SECTION 2: CORE DESIGN PHILOSOPHY & MANDATORY ENGINEERING RULES

### 2.1 Core Design Objectives
1. TypeScript/Python-inspired developer-friendly syntax and type safety.
2. C-level low-level hardware and memory control.
3. Native compiled performance (targeting performance equal to or better than equivalent optimized C programs on representative workloads, verified via benchmarks).
4. Modern Object-Oriented Programming (OOP) as the primary paradigm.
5. First-class procedural programming support (top-level functions, no forced class wrapping).
6. Practical functional programming support (lambdas, closures, `Map`/`Filter`/`Reduce`, immutability, pattern matching).
7. Unified automatic and manual memory management in the same application.
8. Native pointer manipulation and safe references.
9. Architecture-checked inline assembly execution.
10. Direct hardware access and portable Hardware Abstraction Layer (HAL).
11. Robotics-oriented and AI-ready standard libraries.
12. AI-native language structure (unambiguous grammar, structured metadata, first-class `Explain` blocks, machine-readable AST/diagnostics).
13. Dual execution architecture: fast Bytecode Interpreter for development and optimizing LLVM Native Compiler for production.
14. Mandatory per-file testing (`.test.rbx`) enforced by the build system.
15. Cross-platform and bare-metal target architecture support.

### 2.2 The 20 Mandatory Engineering Rules
1. Never implement fake functionality.
2. Never use placeholder methods for features claimed to be implemented.
3. Never report a test as successful unless it was actually executed.
4. Never claim a feature is supported if it only exists in documentation.
5. Never leave important language behavior undefined.
6. Never introduce inconsistent naming conventions.
7. Never create undocumented syntax.
8. Never silently change existing language semantics.
9. Never break backward compatibility without a documented `LanguageVersion` change.
10. Never sacrifice memory safety without an explicit `Unsafe` boundary.
11. Never allow uncontrolled memory ownership transitions.
12. Never silently ignore compilation errors.
13. Never pretend that hardware is available when it is not.
14. Never use simulated hardware results as real hardware validation.
15. Never sacrifice maintainability for unnecessary complexity.
16. Keep compiler modules independent and testable.
17. Keep Robix and RobixUI specifications synchronized.
18. Provide meaningful diagnostics with source locations (file, line, column, error code, and actionable suggestion).
19. Keep all public APIs documented (enforcing `Explain { }` blocks).
20. Maintain a changelog, version history, and honest feature readiness status report (`Implemented`, `Partially Implemented`, `Planned`, `Unsupported`).

### 2.3 C++ Implementation Engineering Standards
- Implement the entire ecosystem in **Modern C++ (C++20 / C++23)**. Do not mix unrelated languages across major components.
- Prefer RAII, smart pointers, move semantics, `const` correctness, and efficient data structures.
- Avoid unnecessary raw pointers or heap allocations inside the compiler/VM implementation.
- Separate the internal C++ compiler/VM memory management from the Robix language memory model.
- Enable strict compiler warnings (`-Wall -Wextra -Wpedantic` / `/W4`), use sanitizers (ASan, UBSan, TSan) during development, and profile before optimizing.

---

## SECTION 3: REPOSITORY & PROJECT ARCHITECTURE

### 3.1 C++ Implementation Repository Layout (`Robix/`)
```text
Robix/
├── CMakeLists.txt
├── Compiler/
│   ├── Lexer/
│   ├── Parser/
│   ├── AST/
│   ├── Semantic/
│   ├── Types/
│   ├── IR/
│   ├── Optimizer/
│   ├── CodeGen/
│   └── Linker/
├── Interpreter/
│   ├── Bytecode/
│   ├── VM/
│   ├── Execution/
│   ├── Objects/
│   ├── Memory/
│   └── Debugging/
├── Runtime/
│   ├── Core/
│   ├── Memory/
│   ├── Threading/
│   ├── IO/
│   ├── Hardware/
│   └── Robotics/
├── RobixUI/
│   ├── Parser/
│   ├── Components/
│   ├── Layout/
│   ├── Styling/
│   ├── Binding/
│   ├── Events/
│   └── Rendering/
├── RobixBuild/
│   ├── Parser/
│   ├── Schema/
│   ├── Graph/
│   ├── Targets/
│   ├── Env/
│   └── Runner/
├── Network/
│   ├── Contracts/
│   ├── RPC/
│   ├── Transport/
│   ├── Events/
│   ├── Discovery/
│   └── Security/
├── Packages/
│   ├── Resolver/
│   ├── Registry/
│   └── Manager/
├── Tools/
│   ├── CLI/
│   ├── Formatter/
│   ├── Linter/
│   ├── LSP/
│   ├── Debugger/
│   └── Profiler/
├── StdLib/
├── Specification/
├── Tests/
│   ├── Compiler/
│   ├── Interpreter/
│   ├── Semantics/
│   ├── Memory/
│   ├── Performance/
│   ├── RobixUI/
│   ├── RobixBuild/
│   └── Network/
├── Examples/
└── Documentation/
```

### 3.2 Three Development Modes & Official Project Styles
Robix is a **general-purpose programming language first**, not a mandatory client-server framework. Developers can start with a single `.rbx` file and progressively adopt project configs, `RobixBuild` (`.rbxb`), `RobixUI` (`.rbxu`), and `Robix Network` only when needed.

#### A. The Three Development Modes
1. **Mode 1: Single-File Development (No Project Required)**
   - Write and execute or compile an individual `.rbx` or `.rbxu` file directly (`robix run Hello.rbx`, `robix build Hello.rbx --release`, `robix repl`).
2. **Mode 2: Standard Application Development (`.rbxb` Optional at First)**
   - Create offline games, desktop applications, CLI utilities, libraries, embedded firmware, or OS kernels (`robix new MyGame`, `robix run`, `robix build --release`).
   - carries **zero** unnecessary networking runtime components or client/server configuration.
   - When no `.rbxb` file exists, `robix build` automatically discovers source files, inspects imports, calculates dependency order, and produces the default output.
3. **Mode 3: Network Application Development (Optional Distributed Architecture via `Project.rbxb`)**
   - Create multi-client, multi-server applications with a shared `Common/` contract directory (`robix new MyNetworkApp --template network`, `robix build --all`, `robix run --client Client1`, `robix run --server Server1`).

#### B. Official Project Styles & `.rbxb` Requirement Matrix
| Project Style | Main Purpose | `.rbxb` Required? |
| :--- | :--- | :--- |
| `SingleFile` | Scripts and individual `.rbx` / `.rbxu` programs | **No** |
| `Application` | Desktop and offline applications (small to large) | **No** for small apps; **Optional** for custom builds |
| `Game` | Offline or interactive games (logic, graphics, audio, UI) | **Optional** |
| `Library` | Reusable software libraries and `.rbxp` packages | **Recommended** |
| `Embedded` | Firmware and hardware control software | **Recommended** |
| `System` | Operating-system kernels, bootloaders, and device drivers | **Recommended** |
| `Network` | Multiple clients, multiple servers, and shared `Common/` contracts | **Required** |
| `Custom` | Developer-defined multi-target / multi-configuration arrangements | **Recommended** |

#### C. Standard Robix Project Layout (`MyRobot/`)
Separates build instructions (`Project.rbxb`) from project metadata/dependencies (`Project.rbxproj`):
```text
MyRobot/
├── Project.rbxb                  # Build instructions (RobixBuild)
├── Project.rbxproj               # Project metadata and package dependencies
├── Robix.Lock (or robix.lock)    # Locked dependency versions and hashes
├── Source/
│   ├── Main.rbx
│   ├── Main.test.rbx
│   ├── Robot.rbx
│   ├── Robot.test.rbx
│   ├── Motor.rbx
│   ├── Motor.test.rbx
│   ├── Sensor.rbx
│   └── Sensor.test.rbx
├── UI/
│   └── Dashboard.rbxu
├── Resources/
│   ├── Images/
│   ├── Sounds/
│   └── Configs/
├── .robix/
│   └── packages/
└── Build/
```

#### D. Robix Network Project Layout (`MyApplication/`)
```text
MyApplication/
├── Project.rbxb                  # Orchestrates Common, Clients, Servers, Network Connections & Env
├── Project.rbxproj
├── .env.client                   # Root defaults applied to all Clients (non-secret)
├── .env.server                   # Root defaults applied to all Servers (never bundled into clients)
├── Common/
│   ├── Models/ (User.rbx, Payment.rbx, Notification.rbx)
│   ├── Services/ (UserService.rbx, PaymentService.rbx, NotificationService.rbx)
│   ├── Events/ (UserEvents.rbx, PaymentEvents.rbx)
│   └── Errors/ (NetworkErrors.rbx)
├── Client1/ (Client.rbx, Dashboard.rbxu, .env)
├── Client2/ (Client.rbx, .env)
├── Client3/ (Client.rbx, .env)
├── Server1/ (Server.rbx, UserService.rbx, .env)
├── Server2/ (Server.rbx, PaymentService.rbx, .env)
├── Server3/ (Server.rbx, NotificationService.rbx, .env)
└── Build/
    ├── Client1/Client1.exe
    ├── Client2/Client2.exe
    ├── Client3/Client3.exe
    ├── Server1/Server1.exe
    ├── Server2/Server2.exe
    ├── Server3/Server3.exe
    └── Common/Generated/ (NetworkContracts, SharedTypes)
```

---

## SECTION 4: STAGED DEVELOPMENT PLAN & GOALS

### Release Priorities Overview
- **Priority 1: Robix 1.0 (Core Language & Dual Engine)** — Complete formal specification, syntax, static type system, type inference, memory safety, OOP, procedural & functional programming, `Explain` documentation blocks, mandatory `.test.rbx` testing, bytecode interpreter VM, and LLVM native compiler.
- **Priority 2: Robix 1.x (Complete Ecosystem)** — Package manager (`robix`), `.rbxapi`, `.rbxp` binary packages, built-in JSON/XML/CSV, threading, multiprocessing, async/await, networking, RobixUI, debugger, profiler, LSP, and standard libraries.
- **Priority 3: Robix 2.0 (Advanced Systems & AI)** — Deep robotics algorithms, hard/firm real-time execution profile, AI/ML & GPU acceleration, SIMD, distributed computing, bare-metal/OS development, compile-time metaprogramming, JIT compilation, and formal verification.

---

### ✅ STAGE 1: Formal Language Specification, ABI & Governance (Foundation) — COMPLETED

#### Goals
Establish an unambiguous, complete EBNF grammar, ABI, and semantic specification so that no language behavior depends on undocumented compiler implementation details, and establish language governance and versioning (`LanguageVersion = "1.0"`).

#### Delivered Artifacts & Verified Implementation
- ✅ [`Specification/RobixGrammar.ebnf`](file:///d:/NEW/Robix/Specification/RobixGrammar.ebnf) — Complete EBNF grammar for `.rbx`, `.test.rbx`, `.rbxapi`, and `.rbxb`.
- ✅ [`Specification/RobixUIGrammar.ebnf`](file:///d:/NEW/Robix/Specification/RobixUIGrammar.ebnf) — Complete XML-compatible EBNF grammar for RobixUI (`.rbxu`).
- ✅ [`Specification/LanguageSpecification.md`](file:///d:/NEW/Robix/Specification/LanguageSpecification.md) — Formal specification of lexical rules, Unicode UAX #31 NFC policy, 18-level operator precedence table, exact primitive sizes/alignments, all 10 loop forms, `Iterator<T>` protocol, `Explain` blocks, unified memory model, and OS integration.
- ✅ [`Specification/ABISpecification.md`](file:///d:/NEW/Robix/Specification/ABISpecification.md) — Complete `RobixABI_v1` specification (calling conventions across `x86_64`, `ARM64`, `ARM32`, `RISC-V`, `WASM32`, struct/class layouts, `_RBX1_` symbol mangling, `.rbxp` ABI hashes, and `LanguageVersion = "1.0"` governance).
- ✅ [`Compiler/Types/SpecValidator.hpp`](file:///d:/NEW/Robix/Compiler/Types/SpecValidator.hpp) & [`Compiler/Types/SpecValidator.cpp`](file:///d:/NEW/Robix/Compiler/Types/SpecValidator.cpp) — C++20 specification, naming/acronym, visibility, type-layout, and ABI mangling engine.
- ✅ [`Tests/Semantics/Stage1SpecTests.cpp`](file:///d:/NEW/Robix/Tests/Semantics/Stage1SpecTests.cpp) — 68/68 Stage 1 conformance tests compiled with `clang++ -std=c++20 -Wall -Wextra -Wpedantic` and passing.

#### Detailed Scope & Deliverables
1. ✅ **Lexical & Syntactic Grammar (EBNF) & Unicode Policy**
   - Lexical grammar, UTF-8 source encoding, **Unicode identifier policy** (UAX #31 / NFC normalization for consistent international identifier support while enforcing naming conventions), token definitions, keywords, and operators.
   - **Formal Operator Precedence & Associativity Specification:** Complete, unambiguous precedence table covering unary, arithmetic, bitwise, shift, relational, equality, logical, range (`..`, `..=`), null-coalescing, member access, and assignment operators.
   - Expression grammar, statement grammar, declaration grammar.
   - Class, abstract class, final class, partial class, nested class, struct, enum, interface, and trait grammars.
   - Type grammar, generic parameter and constraint grammar.
   - Function declaration grammar (supporting both C/Java-style `public Int Add(...)` and keyword-style `Function Add(...) -> Int`, optional return types, default arguments, named arguments, variadic arguments, and function overloading).
   - **Attributes, Annotations & Macros Grammar:** `@Attribute` syntax for compiler-recognized and user-extensible metadata, `consteval` / compile-time execution blocks, and hygiene/AST rules for declarative and procedural macros.
   - First-class `Explain { ... }` documentation block grammar inside function and method declarations.
2. ✅ **Loop, Iterator Protocol & Control Flow Specification**
   - **Formal Iterator Protocol (`Iterable<T>` / `Iterator<T>`):** Standardized zero-cost `Next() -> Option<T>` protocol so all built-in and user-defined collections work uniformly with `for item in collection` and functional pipelines.
   - Traditional `for (Int i = 0; i < 10; i++) { ... }`
   - Exclusive range loop: `for i in 0..10 { ... }`
   - Inclusive range loop: `for i in 1..=10 { ... }`
   - Collection iteration: `for name in names { ... }`
   - Enumerated iteration: `for (index, name) in names.Enumerate() { ... }`
   - `while condition { ... }` and `do { ... } while condition;`
   - Fixed repetition: `repeat 5 { ... }`
   - Infinite loop for embedded/OS/robotics: `loop { ... }`
   - `break`, `continue`, `return`, `defer { ... }`, `using resource { ... }`, `goto` (only if justified for low-level systems code), `if / else if / else`, `switch` / `match` pattern matching.
3. ✅ **Module Initialization, Package Visibility & Flexible `Main()` Specification**
   - **Module Initialization Rules:** Deterministic, cycle-free initialization order for package-level constants and static state prior to `Main()` execution, guaranteeing predictable program startup.
   - **Package & Module Visibility Rules:** Exact access boundaries for `public`, `private`, `protected`, `internal`, and `package` across modules, files, and `.rbxapi` boundaries.
   - **Flexible `Main()` Entry Point:**
     - Support top-level `Main()` or `Program.Main()`.
     - `Main()` / `Main() -> Void`: Normal completion returns exit code `0`.
     - `Main() -> Int`: Returned integer becomes the OS process exit code (`0` success, `1` general failure, `2` invalid args, `3` config error, `4` hardware init failure, etc.).
     - `Main() -> String`, `Main() -> Array<T>`, `Main() -> Object` (anonymous/structured value like `{ Name: robot, Battery: 95 }`): Displays/exposes structured output and exits with `0` (plus a separate Runtime API to set explicit OS exit code if needed).
     - `Main() -> Result<T, E>`: `Success(value)` outputs value and exits `0`; `Error(err)` reports error and exits with non-zero code.
     - Inferred return type for `Main()` when all return paths resolve to one compatible static type.
4. ✅ **Complete ABI & Binary Compatibility Specification (Critical)**
   - **Robix Calling Conventions:** Register usage, stack frame layout, parameter passing, and return value placement across `x86_64`, `ARM64`, `ARM32`, `RISC-V`, and `WASM32`.
   - **Object & Struct Memory Layouts:** Deterministic field ordering, alignment, padding, vtable pointers, and packed struct layouts (`@Packed`, `@Align(N)`).
   - **Name Mangling Specification:** Canonical, reversible symbol mangling encoding package, namespace, type/class, function name, parameter types, and ABI version.
   - **Generic Type ABI Rules:** Monomorphized symbol generation, cross-package generic instantiation, and `.rbxapi` template metadata rules.
   - **Exception & `Result` Interoperability:** Zero-cost unwinding tables and safe boundary rules across native/C FFI and package boundaries.
   - **Binary Compatibility & Plugin ABI:** Stable plugin and dynamic library (`.dll` / `.so` / `.dylib`) ABI across compiler versions, backed by an automated **ABI Compatibility Test Suite**.
5. ✅ **Semantic, Undefined Behavior, Resource Lifetime & Governance Rules**
   - Evaluation order, integer overflow rules, floating-point behavior (IEEE-754, radians vs. degrees), and **strict Undefined Behavior (UB) rules** (safe Robix code has **zero** undefined behavior; all potentially UB operations are confined to `Unsafe` with documented contracts).
   - **Resource Lifetime Rules:** Formal lifetime rules for safe hardware handles (`GPIO`, `PWM`, `DMA`), file descriptors, sockets, and `RobixUI` components/bindings.
   - **Stack & Heap Limits:** Compile-time and runtime configurable stack size and heap allocation budgets for embedded, real-time, and safety-critical targets.
   - Compatibility policy, deprecation policy, and public proposal process.

---

### ✅ STAGE 2: Shared C++ Frontend (Lexer, Parser, AST, Type System & `Explain` Validator) — COMPLETED

#### Goals
Implement a single, shared C++ frontend used by both the Bytecode Interpreter and the Native Compiler so that language rules are never duplicated or divergent.

#### Detailed Scope & Deliverables
1. ✅ **Source Loader, Lexer & Token System** (`Compiler/Lexer/Token.hpp`, `Compiler/Lexer/Lexer.hpp`, `Compiler/Lexer/Lexer.cpp`)
   - Fast UTF-8 source loader with source location tracking (file, line, column, span).
   - Token system covering all keywords, literals (integers, floats, strings, interpolated strings `$"Hello {name}"`, multiline strings `"""..."""`, arrays, booleans), operators, and identifiers.
   - Naming convention and acronym capitalization checker integrated at AST/semantic level.
2. ✅ **Parser & Abstract Syntax Tree (AST)** (`Compiler/AST/ASTNodes.hpp`, `Compiler/Parser/Parser.hpp`, `Compiler/Parser/Parser.cpp`)
   - Recursive descent / Pratt parser generating a strongly typed, immutable/arena-allocated C++ AST.
   - Full support for packages, imports, classes, structs, enums, interfaces, traits, functions, `Explain` blocks, statements, expressions, pattern matching, destructuring, attributes (`@RealTime`, `@Priority`, `@Period`), capabilities (`requires Hardware.Camera`), and `Test` / `TestFile` declarations.
3. ✅ **Complete Static Type System & Type Inference** (`Compiler/Types/TypeChecker.hpp`, `Compiler/Types/TypeChecker.cpp`)
   - **Primitive & Numeric Types** (with exact sizes, signedness, ranges, and alignment):
     - `Bool` (1 byte), `Byte` (8-bit), `Char` (UTF-8 / Unicode scalar)
     - Signed integers: `Int8`, `Int16`, `Int32`, `Int64`, `Int` (platform-sized)
     - Unsigned integers: `UInt8`, `UInt16`, `UInt32`, `UInt64`, `UInt` (platform-sized)
     - Floating-point & Decimal: `Float16`, `Float32`, `Float64`, `Float` (default float), `Double`, `Decimal` (high-precision decimal arithmetic)
     - `String` (UTF-8), `Void`
   - **Compound & Advanced Types:**
     - Fixed-size arrays, dynamic `Array<T>`, multidimensional arrays, `Span<T>` / slices, `Tuple`, `Struct`, `Enum`, `Class`, `Interface`, `Trait`, `Union` types, type aliases, refinement types, compile-time types.
     - Null safety & optionals: Nullable `T?` (requiring explicit null check before access) and `Option<T>`.
     - Error types: `Result<T, E>` (`Success(val)` / `Error(err)`).
     - Function types, delegates, events, closures, and lambdas (`n => n * 2`).
     - Pointers & References: Safe references (`&T`, `&mut T`) vs. raw pointers (`Pointer<T>`, const/mutable/null pointers, function pointers, pointer-to-pointer).
     - Generics: Generic classes, structs, interfaces, and functions with constraints and monomorphization/specialization.
   - **Automatic Type Inference (`let`, `var`, `const`):**
     - `let`: Immutable inferred binding (`let name = "Roboticela";`, `let speed: Float = 100;`).
     - `var`: Mutable inferred binding (`var battery = 100;`). Retains static type; reassigning a different type is a compile-time error.
     - `const`: Compile-time constant (`const Int MAX_SPEED = 500;`).
     - Explicit type declarations (`Int age = 25;`) produce identical operations to inferred declarations.
     - Inferred function return types when unambiguous.
   - **Strict Type Safety:**
     - Reject implicit narrowing or unrelated conversions (e.g., `Int + Float` is rejected; requires `speed.ToFloat() + temperature`).
     - Definite assignment analysis and const correctness.
4. ✅ **First-Class `Explain { }` Documentation Block Validation** (`Compiler/Types/TypeChecker.cpp`)
   - Support both multiline and compact `Explain` syntax inside functions/methods:
     - `Description` (Required)
     - `Input` (Required for every parameter; compiler validates parameter names against signature)
     - `Processing` (Required)
     - `Output` (Required for non-`Void` functions)
     - `Errors` (Required when returning `Result` or throwing errors)
     - `SideEffects` (Required when mutating state/hardware/IO)
   - Emit structured warnings/errors (e.g., `Warning D3001: Incomplete function explanation`, blocking build in strict documentation mode and required for all `.rbxapi` public functions).
   - Zero runtime overhead (stripped from executable code paths, preserved in metadata/`.rbxapi`/docs).
5. ✅ **Diagnostics System** (`Compiler/Semantic/Diagnostics.hpp`, `Compiler/Semantic/Diagnostics.cpp`, `Tests/Compiler/Stage2FrontendTests.cpp`)
   - Structured human-readable and machine-readable (JSON/SARIF) diagnostics with error codes (`R1001`, `R1004`, `D3001`, `R5001`), exact file/line/column spans, expected vs. received types, and actionable fix suggestions.

---

### ✅ STAGE 3: Core Language Semantics (OOP, Procedural, Functional & Error Handling) — COMPLETED

#### Goals
Implement the full semantic analyzer, symbol table, access control, OOP object model, procedural execution, functional constructs, and deterministic resource management.

#### Detailed Scope & Deliverables
1. ✅ **Object-Oriented Programming (Primary Paradigm)** (`Compiler/Semantic/SemanticAnalyzer.hpp`, `Compiler/Semantic/SemanticAnalyzer.cpp`)
   - Classes, objects (`new ClassName(...)`), constructors, constructor chaining (`: Base(...)`), destructors.
   - Fields, properties, instance methods, static members, operator overloading (with strict rules), extension methods.
   - Access modifiers: `public`, `private`, `protected`, `internal`, `package` with strict visibility enforcement.
   - Single inheritance (`extends` / `:`), `abstract` classes and methods, `override` enforcement, method hiding rules, method overloading, `final` classes.
   - Interfaces and traits implementation, polymorphism, virtual dispatch tables (vtables), devirtualization analysis.
   - Nested classes and partial classes.
2. ✅ **Procedural Programming** (`Compiler/Semantic/SemanticAnalyzer.cpp`)
   - Top-level functions, procedures, module-level constants, and standalone executable entry points without requiring wrapper classes.
   - Named arguments (`MoveRobot(speed: 50.0, emergencyStop: true)`), default parameter values, variadic functions, recursion, and nested functions.
3. ✅ **Functional Programming** (`Compiler/Semantic/SemanticAnalyzer.cpp`)
   - First-class function types, anonymous functions, lambdas, closures with environment capture rules.
   - Collection transformations (`Filter`, `Map`, `Reduce`) with stream fusion / allocation elimination optimizations.
   - Exhaustive `match` pattern matching on `Result<T, E>`, `Option<T>`, `Enum` variants, values, and destructuring of tuples/structs.
   - Coroutines and lazy generators.
4. ✅ **Error Handling, Resource Lifetimes & Metaprogramming** (`Compiler/Semantic/SemanticAnalyzer.cpp`, `Tests/Semantics/Stage3SemanticTests.cpp`)
   - Recoverable errors via `Result<T, E>` (`Success` / `Error`) and `Option<T>` / `T?`.
   - Exceptional failures via `try / catch / finally` and `throw`, plus unrecoverable `Panic` handling and hardware fault reporting.
   - Deterministic cleanup via `defer { ... }` and `using resource { ... }` (RAII), guaranteed to run on scope exit, early return, `Result` error propagation, exception unwinding, or task cancellation.
   - **Compile-Time Execution (`consteval`), Reflection, Attributes & Macros:**
     - `consteval` functions and compile-time evaluation with bounded step/memory limits.
     - Opt-in static and runtime **Reflection** (`TypeInfo`, field/method/attribute inspection) with zero runtime cost on types that do not use runtime reflection.
     - **Attributes & Annotations** (`@RealTime`, `@Priority`, `@Deprecated`, custom user attributes) and hygienic **Macros** for safe compile-time code generation.

---

### ✅ STAGE 4: Dual Execution Architecture, SSA IR & Compiler Verification (Bytecode VM & Native LLVM Compiler) — COMPLETED

#### Goals
Build both first-class execution engines on top of a shared **SSA-based Validated Intermediate Representation (IR)**, ensuring identical semantics between development (`robix run`) and production (`robix build --release`), while enforcing rigorous compiler optimization verification.

#### Detailed Scope & Deliverables
1. ✅ **Shared SSA-Based Validated Intermediate Representation (IR) & Pass Manager** (`Compiler/IR/SSAIR.hpp`, `Compiler/IR/SSAIR.cpp`, `Compiler/Optimizer/IRPassManager.hpp`, `Compiler/Optimizer/IRPassManager.cpp`)
   - Lowering from type-checked AST to a **Static Single Assignment (SSA) High-Level Typed IR** for ownership, lifetime, definite assignment, and escape analysis, before branching to the Bytecode Generator and the LLVM IR Code Generator.
   - **Compiler Pass Dependency Management:** Explicit pass manager scheduling analysis passes, transformation passes, and invalidation rules deterministically.
   - **Optimization Correctness Verification:** IR invariants verifier run after every transformation pass (verifying SSA dominance, type consistency, ownership invariants, and absence of compiler-introduced Undefined Behavior).
   - **Compile-Time Evaluation Limits:** Configurable recursion depth, step count, and memory limits for `consteval` and macro expansion to prevent infinite compile-time loops.
   - **Compiler Crash Recovery & Diagnostic Reporting:** Fault-tolerant parser/semantic recovery, internal compiler error (ICE) crash handlers with clean diagnostic bug reports and reproducer generation.
2. ✅ **Robix Bytecode Specification & Virtual Machine (Development Engine)** (`Interpreter/Bytecode/BytecodeModule.hpp`, `Interpreter/Bytecode/BytecodeGenerator.cpp`, `Interpreter/VM/BytecodeVM.hpp`, `Interpreter/VM/BytecodeVM.cpp`)
   - **Instruction Set:** Constants, variables, arithmetic, comparisons, branches, function calls, returns, object creation, field access, method/virtual calls, arrays/collections, memory allocation/access, pointer operations, exceptions, generic instantiation, native FFI calls, and hardware operations.
   - **Architecture:** Versioned bytecode format with pre-execution verifier, register/stack frame model, calling conventions, exception tables, and source-level debug tables.
   - **Capabilities:** Fast startup for `robix run`, `--watch` mode (revalidating affected modules), interactive `robix repl`, runtime stack traces, breakpoint hooks, variable inspection, and `robix interpreter info|version|trace`.
   - **JIT Readiness:** Design profiling hooks for hot-function detection to allow optional future JIT compilation without sacrificing correctness.
3. ✅ **Robix Native Compiler (Production Engine via LLVM)** (`Compiler/CodeGen/LLVMIRGenerator.hpp`, `Compiler/CodeGen/LLVMIRGenerator.cpp`)
   - Lowering Validated SSA IR to LLVM IR.
   - Generation of native executables, static libraries (`.a` / `.lib`), shared libraries (`.so` / `.dll` / `.dylib`), and object files (`.o` / `.obj`).
   - **Optimization Pipeline:** Constant folding, dead code elimination, function inlining, loop optimization, strength reduction, common subexpression elimination (CSE), escape analysis (converting heap allocations to stack allocations), devirtualization, register allocation, memory layout optimization, SIMD auto-vectorization, Link-Time Optimization (LTO), and Profile-Guided Optimization (PGO).
   - **Link-Time Symbol Resolution & Debug Information Preservation:** Strict cross-module symbol resolution, dead-strip verification, and full preservation of DWARF/PDB debug locations through optimization passes.
   - **Official Build Profiles:**
     - `debug`: Debug symbols (DWARF/PDB), source-level debugging, runtime safety diagnostics, fast compile.
     - `release`: General optimized production build.
     - `performance`: Maximum optimization (`-O3`, LTO, aggressive vectorization/inlining).
     - `realtime`: Deterministic execution, bounded operations, no unpredictable GC pauses.
     - `embedded` / `size`: Minimal binary and memory footprint, minimal runtime.
     - `safety`: Extra static and runtime bounds/overflow/memory/sanitizer checks.
   - **Minimal vs. Full Runtime:** Simple programs compile to native code without paying for unused GC, reflection, or dynamic loading runtime overhead.
4. ✅ **"Write Once, Compile Native Everywhere" — Zero-Cost Compiler OS & Platform Integration (Critical)** (`Compiler/CodeGen/LLVMIRGenerator.cpp`)
   - **Core Principle:** **Developers write OS-independent Robix code once, and the Robix compiler + runtime natively integrate every operating system under the hood with exact C-level performance.** Users never have to write `#ifdef` platform boilerplate for standard system, file, thread, network, or windowing operations.
   - **Direct Compiler & Runtime OS Backends (Zero Wrapper Overhead):**
     - **Windows Backend:** Direct native integration with Win32 / NT APIs, IOCP async I/O, Windows threads/fibers, DirectX/Win32 windowing & input.
     - **Linux Backend:** Direct Linux system calls / `libc`, `io_uring` / `epoll` async I/O, `pthread` / `futex`, Wayland/X11/DRM windowing & input, and `sysfs`/`ioctl` hardware access.
     - **macOS Backend:** Direct Darwin/Mach system calls, `kqueue` / Grand Central Dispatch, Cocoa/Metal native frameworks, and CoreAudio/IOKit integration.
     - **Android Backend:** Direct Android NDK / Bionic libc, JNI-free native activity lifecycle, AAudio, SensorManager, and Vulkan/GLES rendering.
     - **WebAssembly Backend:** Direct `wasm32-wasi` and browser host bindings with linear memory optimization.
     - **Embedded & Bare-Metal Backend:** Direct hardware register / MMIO / interrupt vector table generation with zero OS dependency.
   - **How C-Equivalent Speed Is Preserved Across All OSes:** High-level OS-independent Robix APIs (`File`, `Thread`, `Process`, `Network`, `Robix.UI`, `Hardware`) are monomorphized and inlined at compile time directly to the target OS's native system calls (`CreateFileW` on Windows, `openat`/`io_uring` on Linux, etc.)—with **zero virtual dispatch or runtime abstraction penalty**.
   - Target triples (`windows-x64`, `linux-x64`, `linux-arm64`, `macos-arm64`, `android-arm64`, `riscv64`, `wasm32`, `bare-metal`) remain available when low-level OS-specific drivers or kernel code explicitly use `#if TARGET_...`.
5. ✅ **Incremental Compilation & Build Cache** (`Compiler/CodeGen/LLVMIRGenerator.hpp`, `Compiler/CodeGen/LLVMIRGenerator.cpp`)
   - Track file, module, package version, type, interface, and generic instantiation dependencies.
   - Deterministic build cache (`robix build --incremental`, `robix clean`, `robix cache clear`).
6. ✅ **Single-File / Standalone Execution, Compilation & Testing Mode (Project-Optional)** (`Interpreter/VM/BytecodeVM.cpp`, `Tests/Interpreter/Stage4DualEngineTests.cpp`)
   - Both the **Bytecode Interpreter (`robix`)** and the **Native Compiler (`robixc`)** must support operating directly on an individual `.rbx`, `.test.rbx`, or `.rbxu` file—either standalone (outside any project directory without requiring a `.rbxproj` file) or targeting a single file inside an existing project without building/running/testing the entire project:
     - **Interpreter Single-File Run:** `robix run File.rbx` (or `robix run File.rbx --engine interpreter`) parses, type-checks, and immediately executes `File.rbx` (resolving any relative imports or project-local `.robix/packages` if inside a project).
     - **Native Single-File Compile & Run:** `robixc File.rbx -o OutputBinary`, `robixc run File.rbx`, or `robix run File.rbx --engine native` compiles a single `.rbx` file to native machine code via LLVM and optionally executes it directly.
     - **Single-File RobixUI Execution & Preview:** `robix run Window.rbxu`, `robix ui run Window.rbxu`, `robix ui preview Window.rbxu` (interpreter/hot-reload preview), and `robixc Window.rbxu` / `robix ui build Window.rbxu` (single-file UI compilation/validation) allow running, previewing, and compiling an individual `.rbxu` UI file (optionally paired with its controller `.rbx` via `--controller Controller.rbx` or auto-discovered companion file).
     - **Single-File Testing (Interpreter or Native):** `robix test File.rbx` or `robix test File.test.rbx` (with `--engine interpreter` or `--engine native`) and `robixc test File.rbx` execute **only** the unit/boundary tests for that specific file and its paired `.test.rbx` without running the whole project's test suite.
     - **Single-File Check, Lint, Format & Verify:** `robix check File.rbx`, `robix check Window.rbxu`, `robix lint File.rbx`, `robix fmt File.rbx`, and `robix verify File.rbx` (verifies `File.rbx` + `File.test.rbx` + `Explain` blocks in isolation).
7. ✅ **Semantic Conformance Guarantee** (`Tests/Interpreter/Stage4DualEngineTests.cpp`)
   - Shared conformance test suite running every semantic test on both `--engine interpreter` and `--engine native` to verify identical behavior.
8. ✅ **How Robix Achieves C-Equivalent Native Performance (Architectural Matrix)** (`Compiler/Optimizer/IRPassManager.cpp`, `Compiler/CodeGen/LLVMIRGenerator.cpp`)
   - In **Native Production Mode (`--release` / `--profile performance`)**, Robix compiles directly to native machine code via LLVM to match or exceed optimized C on equivalent workloads:
     - **Zero Mandatory VM/GC Overhead:** No virtual machine or mandatory garbage collector in native builds; deterministic destruction/RAII and zero-cost abstractions compile away to direct machine instructions.
     - **Monomorphized Generics & Static Dispatch:** Generics specialize to concrete machine types (no boxing), and devirtualization converts virtual calls to direct/inlined calls whenever concrete types are known.
     - **Value Types, Stack Promotion & Arena Allocation:** Structs, `Span<T>` slices, escape analysis, and `Arena` allocators eliminate unnecessary heap allocations and pointer chasing.
     - **Strict Aliasing, SIMD & Inline `ASM`:** Ownership rules give the LLVM backend stronger aliasing guarantees (enabling aggressive loop optimizations and SIMD auto-vectorization), while `Unsafe` pointers and inline `ASM` allow direct hardware/register tuning.

---

### ✅ STAGE 5: Unified Memory Architecture, GC/RC/Manual Coexistence, Pointers & Low-Level Systems — COMPLETED

#### Goals
Deliver **simultaneous automatic memory management AND full manual memory control** in the same application while guaranteeing **exact C-level native performance**—with zero stop-the-world pauses on hot paths, explicit ownership transfer rules, cycle handling, and low-level pointer/assembly access.

#### Detailed Scope & Deliverables
1. ✅ **Unified Memory Model: Simultaneous Automatic + Manual Memory with C-Level Speed (Critical)** (`Runtime/Memory/UnifiedMemory.hpp`, `Runtime/Memory/UnifiedMemory.cpp`)
   - **Coexistence Architecture:** A single Robix application can simultaneously use stack values, automatically managed heap objects, arena/pool regions, and manually allocated `Unsafe` raw memory without interference or runtime penalties on manual/stack code:
     - **Tier 1 — Zero-Cost Stack & Static Promotion (Default Where Possible):** Value types (`Struct`), non-escaping objects (via compiler escape analysis), and local buffers are allocated directly on the CPU stack with **zero** heap or reference-counting cost.
     - **Tier 2 — Deterministic Automatic Memory (Compile-Time Ownership + Non-Atomic/Atomic RC + `Weak<T>`):**
       - Single-owner objects are freed deterministically at scope exit (zero runtime RC overhead, identical to C `free` / C++ `unique_ptr`).
       - Shared objects use high-performance reference counting (non-atomic RC for thread-local objects; atomic RC only when shared across threads).
       - **Weak References (`Weak<T>`) & Reference Cycle Detection:** Provide `Weak<T>` (upgrading safely to `Option<T>`) to prevent reference cycles in graphs/trees/UI, plus an optional background/incremental cycle collector or static cycle lint check.
       - **Optional Tracing / Region Collector Mode:** Configurable lightweight cycle/tracing collector for high-level dynamic workloads, completely disabled in `realtime`, `embedded`, and `performance` profiles.
     - **Tier 3 — Scoped Regions & Arenas (`Arena`, `Pool`, `DMA`):** `Arena.Create(1.MB)` and `Pool<T>` provide $O(1)$ bump-pointer allocation and bulk deallocation at scope exit, with compile-time lifetime checks preventing references from escaping the arena.
     - **Tier 4 — Full Manual Memory Control (`Unsafe`):** Direct `Memory.Allocate<T>(count)`, `Memory.AlignedAllocate<T>(count, alignment)`, `Memory.Free(ptr)`, placement construction, explicit memory layouts, packed structures, and bit fields—compiling to raw C-speed allocator calls with zero automatic-runtime tracking overhead.
   - **Precise Ownership Transfer Rules Between Automatic & Manual Memory:**
     - `Memory.IntoRaw(managedObject) -> Pointer<T>`: Explicitly transfers ownership from automatic management to manual control (preventing automatic cleanup from freeing it).
     - `Memory.FromRaw(rawPointer) -> T` (inside `Unsafe`): Explicitly transfers manually allocated memory into automatic lifetime management.
     - Without explicit transfer, the compiler strictly forbids calling `Memory.Free()` on automatically managed objects and forbids automatic cleanup from touching manual pointers.
   - **Destruction Ordering, Shared Memory Model & Failure Handling:**
     - **Deterministic Object Destruction Ordering:** Reverse declaration order for local scope variables and `defer` blocks; base class destructors run after derived class destructors; field destructors run in reverse field declaration order.
     - **Thread-Safe Memory Allocation & Shared Object Model:** Per-thread lock-free magazine/slab caches (like `jemalloc`/`mimalloc`) to eliminate lock contention across threads, with strict `Send`/`Sync`-style thread-sharing rules.
     - **Stack Overflow, Guard Pages & Allocation Failure Handling:** Configurable stack/heap limits, hardware guard pages (or compile-time stack depth verification on bare-metal), and non-panicking `Memory.TryAllocate<T>() -> Result<Pointer<T>, MemoryError>` for safety-critical and embedded systems.
   - **Interpreter Parity:** Support automatic, `Weak<T>`, arena, and manual memory faithfully in the bytecode VM; emit clear diagnostics if a hardware-specific raw physical address operation requires `--engine native`.
2. ✅ **Pointers, References & Slices** (`Runtime/Memory/LowLevelSystems.hpp`)
   - Safe references (`&`, `&mut`) and bounds-checked `Span<T>` / slices in safe code.
   - Raw typed pointers (`Pointer<T>`, `Pointer<Byte>`), address-of (`&value`), dereferencing (`*ptr`), pointer indexing (`ptr[i]`), pointer arithmetic, pointer casting, null pointers, pointer-to-pointer, and function pointers restricted to `Unsafe { ... }` blocks.
3. ✅ **Inline Assembly (`ASM`)** (`Runtime/Memory/LowLevelSystems.hpp`, `Runtime/Memory/LowLevelSystems.cpp`)
   - `Unsafe { ASM("NOP", ...); }` with target architecture validation (`x86_64`, `ARM64`, `ARM32`, `RISC-V`).
   - Support register constraints, input/output operands, clobber declarations, memory effects, `volatile` barriers, and assembler integration.
4. ✅ **Memory-Safe C / Native Interoperability (FFI Boundaries)** (`Runtime/Memory/LowLevelSystems.hpp`, `Runtime/Memory/LowLevelSystems.cpp`, `Tests/Memory/Stage5MemoryTests.cpp`)
   - `Import Native.C;` and `Extern Function` declarations (including variadic C functions like `printf`).
   - **Memory-Safe FFI Boundary Rules:** Explicit pinning/borrowing of Robix buffers passed to C, null-pointer validation at FFI boundaries, exception/panic isolation across C frames, exporting Robix functions with C ABI, and binding generation from C headers.

---

### STAGE 6: Mandatory Testing System & Verification Engine

#### Goals
Make testing a first-class, compiler-enforced requirement of the Robix language so that every `.rbx` production source file is verified by a corresponding `.test.rbx` file.

#### Detailed Scope & Deliverables
1. **Language-Level Test Syntax**
   - Built-in `Test "description" { ... }` blocks and `TestFile "Name.test.rbx" { Covers = "Name.rbx"; ... }` declarations for constants/declaration-only files.
2. **`Assert` Standard Framework (`Robix.Testing`)**
   - `Assert.Equal()`, `Assert.NotEqual()`, `Assert.True()`, `Assert.False()`, `Assert.Null()`, `Assert.NotNull()`, `Assert.Throws()`, `Assert.ThrowsAsync()`, `Assert.Success()`, `Assert.Error()`, `Assert.Contains()`, `Assert.Timeout()`.
3. **Mandatory Per-File Enforcement (`R5001`)**
   - Configurable in `.rbxproj` (`RequireTestFile = true`, `RequireTestsToPass = true`, `RequireTestsForAllSourceFiles = true`, `AllowUntestedDevelopmentBuild = false`, `AllowUntestedReleaseBuild = false`, `EnableCoverageReport = true`).
   - Block builds with `ROBIX TEST ERROR R5001` if any `File.rbx` lacks `File.test.rbx`, if the test file is empty/does not exercise the source file, or if any test fails.
4. **Advanced Testing Capabilities (Project-Wide & Single-File)**
   - **Single-File Isolation Testing:** Run tests for just one `.rbx` or `.test.rbx` file in either Interpreter or Native Compiler mode (`robix test File.rbx`, `robix test File.test.rbx --engine interpreter`, `robix test File.rbx --engine native`, or `robixc test File.rbx`) without executing the entire project's test suite.
   - **Test Categories:** Unit tests, boundary tests, negative compilation tests, integration tests, performance/allocation tests, concurrency/stress tests (`robix test --stress`, `--repeat 100`), memory leak/ownership tests (`robix test --memory-check`), property-based testing, fuzz testing, mutation testing, snapshot testing, hardware simulation tests, and differential testing (Interpreter vs. Native).
5. **Single-Command Project or Single-File Verifier (`robix verify`)**
   - Supports verifying the entire project (`robix verify`) or a single file in isolation (`robix verify File.rbx` / `robix verify Window.rbxu`).
   - Executes syntax check, type check, `Explain` documentation check, naming/style check, missing `.test.rbx` check, test execution, static analysis, and memory safety audit in one report with non-zero exit code on failure.

---

### STAGE 7: Built-In Types, Standard Library & Concurrency

#### Goals
Provide a rich, modular, zero-overhead standard library covering everyday application needs, high-concurrency servers, data processing, and scientific computing.

#### Detailed Scope & Deliverables
1. **Built-In Console I/O (`Print` & `Input`)**
   - `Print(...)` for all built-in and printable types.
   - `Input()` returning `String`, `Input<T>(prompt)` for validated typed input (`Input<Int>`, `Input<Float>`, `Input<Bool>`, `Input<String>`), and `TryInput<T>(prompt)` returning `Result<T, InputError>`.
2. **Built-In Primitive Methods & `Robix.Math`**
   - Numeric methods: `.Abs()`, `.ToFloat()`, `.ToInt()`, `.ToString()`, `.Round()`, `.Floor()`, `.Ceiling()`, `.Sqrt()`, `.Pow()`, `.IsNaN()`, `.IsInfinite()`.
   - `Math` module: `Abs`, `Sqrt`, `Pow`, `Min`, `Max`, `Clamp`, `Sin`, `Cos`, `Tan`, `Random`, `ReLU`, vectors, matrices, quaternions.
   - String methods & **First-Class Regex (`Robix.Regex`)**: `.Length()`, `.ToUpper()`, `.ToLower()`, `.Trim()`, `.Contains()`, `.StartsWith()`, `.EndsWith()`, `.Replace()`, `.Split()`, `.Substring()`, `.IndexOf()`, `.IsEmpty()`, `.ToInt()`, `.ToFloat()`, string interpolation `$"..."`, and compiled DFA/NFA regular expressions (`Regex.Match`, `Regex.FindAll`, `Regex.Replace`, compile-time validated regex literals) with linear-time $O(N)$ execution guarantees (immune to ReDoS).
   - Boolean methods: `.Not()`, `.And()`, `.Or()` alongside `&&`, `||`, `!`.
   - `Robix.Time`: `Date.Today()`, `Time.Now()`, `DateTime.Now()`, `.Year/.Month/.Day`, `.Hour/.Minute/.Second`, `.AddDays()`, `.AddHours()`, `.ToUnixTime()`, `Time.Sleep(Milliseconds(...))`, plus monotonic clocks and high-resolution timers for real-time robotics.
3. **Collections & Formal Iterator Protocol (`Robix.Collections`)**
   - `Array<T>`, `List<T>`, `Dictionary<K, V>`, `HashMap<K, V>`, `SortedMap<K, V>`, `Set<T>`, `HashSet<T>`, `Stack<T>`, `Queue<T>`, `Deque<T>`, `LinkedList<T>`, `PriorityQueue<T>`, `Tree<T>`, `Graph<T>`, `Span<T>`, `Matrix<T>`, `Tensor<T>`.
   - All collections implement the zero-cost `Iterable<T>` / `Iterator<T>` protocol (`Next() -> Option<T>`) and standard methods: `Add`, `Remove`, `Get`, `Set`, `Push`, `Pop`, `Enqueue`, `Dequeue`, `Contains`, `Find`, `Filter`, `Map`, `Reduce`, `Sort`, `Reverse`, `Clear`, `Count`, `IsEmpty`, `Enumerate`.
4. **First-Class JSON (`Robix.Json`) & Serialization**
   - `Json.Parse()`, `Json.TryParse()`, `Json.Serialize<T>(obj, JsonOptions.Pretty | Compact)`, `Json.Deserialize<T>(str)` (with strict type validation), `JsonSchema.Load().Validate()`, streaming JSON, and `.jsonl`.
   - Additional formats: `Robix.Xml`, `Csv`, `Toml`, `Yaml`, binary serialization, Protocol Buffers, MessagePack, `Robix.Database` (SQLite & DB drivers), `Robix.Compression`, `Robix.IO` (OS-independent files, streams, memory-mapped files, and cross-platform filesystem APIs).
5. **Threading, Async/Await Cancellation & Multiprocessing (`Robix.Threading`, `Robix.Process`)**
   - **Multithreading:** `Thread.Start()`, `.Join()`, `Task.Run()`, `Task<T>.Wait()`, `Task.WaitAll()`, `ThreadPool`, `Atomic<T>`, `Mutex` (`lock.WithLock(...)`), `ReadWriteLock`, `Semaphore`, `ConditionVariable`, `Channel<T>`, `Barrier`, `CancellationToken`, thread-safe collections, and compile-time/runtime data-race & deadlock detection.
   - **Async/Await & Structured Cancellation Semantics:** `Async Function`, `await`, non-blocking state-machine tasks without spawning OS threads per operation, plus **structured concurrency and deterministic async cancellation semantics** (`CancellationToken`, automatic cancellation of child tasks when parent scope exits or times out, and guaranteed execution of `defer`/`using` blocks on cancelled async tasks so no tasks or resources are ever abandoned).
   - **Multiprocessing:** Cross-platform `Process.Start()`, `Process.Launch("RobotWorker.rbx", args)`, `.Wait()`, standard I/O pipes, exit codes, IPC, shared memory regions, message queues, named pipes, process isolation, and timeouts.
6. **Formal Security Architecture, Networking & Cryptography (`Robix.Security`, `Robix.Network`, `Robix.Logging`, `Robix.Diagnostics`)**
   - **Formal Threat Modeling & Security Boundaries:** Defined trust boundaries between safe code, `Unsafe` blocks, FFI calls, third-party `.rbxp` packages, network inputs, and hardware peripherals.
   - **Sandboxed Execution & Capability Permissions:** Compile-time and OS-enforced runtime capability model (`requires Hardware.Camera`, `requires Network.Client`, `requires FileSystem.Read`) and sandboxed VM/native execution profiles.
   - **Secure Secret Storage & Cryptography:** Zero-on-drop `Secret<T>` memory buffers (locked in RAM via `VirtualLock`/`mlock` to prevent swapping or crash-dump leakage), constant-time comparisons, CSPRNG, SHA-256/SHA-3, AES-GCM/ChaCha20-Poly1305, Ed25519/ECDSA, and TLS 1.3.
   - **Supply-Chain & Dependency Security:** Cryptographic package signing, reproducible build verification, SBOM generation, automated dependency vulnerability scanning (`robix audit`), and continuous security audit / fuzz / penetration testing suites.
   - **Networking:** OS-independent TCP, UDP, HTTP/HTTPS, WebSockets, DNS, TLS, IPv4/IPv6, Serial, Bluetooth/BLE, MQTT, and gRPC.

---

### STAGE 8: Package Ecosystem & Binary Distribution (`.rbxapi`, `.rbxp`, `robix`)

#### Goals
Implement a secure, project-local binary package manager where packages distribute compiled target binaries plus a documented `.rbxapi` public interface without exposing private implementation source code.

#### Detailed Scope & Deliverables
1. **Public API Definition (`.rbxapi`)**
   - Contains package version, minimum compiler version, imports, capability permissions (`Permissions { Hardware.GPIO; ... }`), and public class/struct/enum/interface/function declarations.
   - Enforces `Explain { }` documentation blocks on all public functions and parameters.
2. **Compiled Package Archive (`.rbxp`)**
   - Archive structure containing:
     - `<Package>.rbxapi`
     - `binaries/` (`linux-x64/*.so`, `windows-x64/*.dll`, `macos-arm64/*.dylib`, `embedded-arm64/*.a`, plus optional portable Robix bytecode artifact)
     - `package.json` (metadata, dependencies, compiler ABI, target list)
     - Cryptographic publisher signature, SHA-256 hashes, license, and SBOM (Software Bill of Materials)
     - `documentation/` and optional separate debug symbols.
3. **Project-Local Package Manager (`robix`)**
   - Installs packages into `<ProjectRoot>/.robix/packages/<Name>/<Version>/`.
   - Reads `Robix.Project.rbxproj` (`robix.project`) and writes deterministic `Robix.Lock` (`robix.lock`).
   - Commands: `robix new` (with `--template robotics|desktop|ai|embedded|library`), `robix init`, `robix add <Pkg>[@version]`, `robix remove`, `robix update`, `robix install`, `robix restore`, `robix list`, `robix tree`, `robix search`, `robix info`, `robix audit`, `robix pack`, `robix verify-package`, `robix sign`, `robix publish`.
4. **Security & ABI Verification**
   - Verify package signature, API hash, target OS/CPU architecture, and compiler ABI compatibility before linking. No arbitrary install script execution by default.

---

### STAGE 9: Hardware Access, Real-Time Execution, Robotics & AI Libraries

#### Goals
Fulfill Robix's core mission as *"The Language of Intelligent Machines"* by providing portable and low-level hardware access, real-time control guarantees, robotics algorithms, and AI/ML integration.

#### Detailed Scope & Deliverables
1. **Hardware Abstraction & Low-Level I/O (`Robix.Hardware`, `Robix.Embedded`)**
   - Portable HAL and low-level drivers for: `GPIO`, `PWM`, `UART`, `SPI`, `I2C`, `CAN`, `USB`, `PCIe`, `DMA`, hardware timers, interrupts, memory-mapped I/O (MMIO), and CPU registers.
   - Respect OS permissions, memory ordering, volatile semantics, and target hardware availability (never fake hardware support on unsupported targets; provide explicit simulation backends for testing).
2. **Real-Time Execution Profile (`Robix.RealTime`)**
   - Annotations and scheduling: `@RealTime`, `@Priority(n)`, `@Period(10.Milliseconds)`, deadline-aware tasks, priority-inheritance mutexes, lock-free/wait-free queues, deterministic arena/pool allocation, No-GC enforcement, watchdog timers, and Worst-Case Execution Time (WCET) measurement/reporting.
3. **Robotics Standard Library (`Robix.Robotics`, `Robix.Sensors`, `Robix.Actuators`, `Robix.Motors`, `Robix.Navigation`, `Robix.Localization`, `Robix.ControlSystems`, `Robix.Communication`)**
   - Sensor interfaces (distance, IMU, LiDAR, encoders, cameras), motor controllers (DC, stepper, servo, brushless), actuator limits, emergency stop (`E-Stop`) and safety monitors.
   - Control systems: PID controllers, state-space controllers, filters (Kalman, complementary).
   - Kinematics (forward and inverse), trajectory and motion planning, coordinate transformations (frames, matrices, quaternions), SLAM/localization helpers.
4. **AI, Machine Learning & Scientific Computing (`Robix.AI`, `Robix.MachineLearning`, `Robix.NeuralNetworks`, `Robix.ComputerVision`, `Robix.Audio`, `Robix.GPU`, `Robix.Science`)**
   - `Matrix<T>` and `Tensor<T>` with SIMD and GPU acceleration.
   - Neural network layers/activations (`Math.ReLU`, etc.), model loading and async/sync inference (`AI.LoadModel(...)`, `model.PredictAsync()`), automatic differentiation, computer vision (`Robix.Imaging`), signal and audio processing.

---

### STAGE 10: RobixUI Declarative User Interface Ecosystem (`.rbxu`)

#### Goals
Implement RobixUI as a native, XML-inspired declarative UI markup language (`.rbxu`) with type-checked data and event bindings to Robix classes.

#### Detailed Scope & Deliverables
1. **RobixUI Parser & Validator**
   - Strict XML-compatible parser for `.rbxu` UI markup files.
   - Incremental validation and UI binary resource caching for production builds.
2. **Component Architecture (`Robix.UI`)**
   - `Window`, `Page`, `View`, `Panel`, `Container`, `Column`, `Row`, `Grid`, `Stack`, `Button`, `Text`, `TextField`, `Image`, `Icon`, `List`, `Table`, `Menu`, `Dialog`, `Tabs`, `Slider`, `Switch`, `Checkbox`, `RadioButton`, `ProgressBar`, `Chart`, `Canvas`, plus custom reusable components, slots, and templates.
3. **Layout & Styling Engines**
   - **Layouts:** Horizontal (`Row`), Vertical (`Column`), `Grid`, `Stack`, absolute, responsive/flex constraints, alignment, margins, padding, spacing, min/max sizes.
   - **Styling:** Colors, typography, fonts (`FontSize`, `FontWeight`), borders, border radius, shadows, backgrounds, gradients, animations, transitions, light/dark themes.
4. **Type-Checked Data Binding & Events**
   - One-way, two-way, computed, collection, and conditional bindings between `.rbxu` components and Robix object properties with defined thread-safety and lifetime rules.
   - Events: `OnClick`, `OnDoubleClick`, `OnMouseEnter`, `OnMouseLeave`, `OnKeyDown`, `OnKeyUp`, `OnFocus`, `OnBlur`, `OnValueChanged`, `OnLoaded`, `OnClosed` bound directly to type-checked Robix controller methods.
5. **Pluggable Rendering Runtime, Single-File Execution & CLI Tools**
   - Clean separation of document parser, component tree, layout engine, styling engine, event dispatcher, and platform rendering backend.
   - **Single-File `.rbxu` Execution & Compilation:** Run, preview, validate, or compile an individual `.rbxu` UI file directly without building the whole project:
     - Interpreter / Live Preview: `robix run MainWindow.rbxu`, `robix ui run MainWindow.rbxu [--controller MainController.rbx]`, `robix ui preview MainWindow.rbxu [--watch]`
     - Native Compiler / Validator: `robixc MainWindow.rbxu [-o DashboardApp]`, `robix ui build MainWindow.rbxu`, `robix ui validate MainWindow.rbxu`
     - Whole-project UI build: `robix ui build`

---

### STAGE 11: RobixBuild (`.rbxb`) — Declarative, Statically Typed Build System

#### Goals
Implement **RobixBuild** (`.rbxb`), a simple, declarative, statically typed build configuration language that replaces complex Makefiles/CMake for Robix projects while remaining completely optional for single-file and simple standalone projects.

#### Detailed Scope & Deliverables

##### Phase 11.1: `.rbxb` Parser, Typed Schema & Optional Auto-Discovery
- Implement a dedicated parser and static validator for `Project.rbxb` (`RobixBuild/`).
- Enforce **compiler-known typed properties** (not arbitrary untyped strings) for `OS`, `Architecture`, `Profile`, `Output`, and project style.
- **Zero-Configuration Fallback (`.rbxb` Optional):** When a developer runs `robix build` or `robix run` without a `.rbxb` file (e.g., `SingleFile`, small `Application`, or offline `Game`), RobixBuild automatically discovers `.rbx` files, parses `import` graphs, computes compilation order, and builds the default executable.
- Support all 8 official project styles: `SingleFile`, `Application`, `Game`, `Library`, `Embedded`, `System`, `Network`, and `Custom`.

##### Phase 11.2: Source Discovery, Dependency Graph & Explicit `Order` Control
- Support explicit file lists (`Files = ["Main.rbx", "Robot.rbx"]`) and glob patterns (`Files = ["**/*.rbx"]`).
- **Automatic Dependency Calculation:** Build a Directed Acyclic Graph (DAG) from `import` statements automatically.
- **Explicit `Order` Block (When Needed):**
  - `First = ["Sensor.rbx"]`
  - `Before = ["Motor.rbx" -> "Robot.rbx", "Robot.rbx" -> "Main.rbx"]`
  - `Last = ["Main.rbx"]`
- **Circular Dependency Detection:** Reject impossible or circular build orders with clear diagnostics pointing to the exact files and properties involved.
- **Parallel & Incremental Compilation:** Compile independent DAG nodes in parallel across CPU cores and recompile only files affected by changes.

##### Phase 11.3: Output Types, Build Profiles & Resource Bundling
- **8 Official Output Types (`Build.Output`):**
  1. `Executable` — Standalone native application
  2. `Library` — Reusable Robix library
  3. `SharedLibrary` — Dynamically linked native library (`.dll`, `.so`, `.dylib`)
  4. `StaticLibrary` — Statically linked native library (`.lib`, `.a`)
  5. `Firmware` — Embedded device firmware image
  6. `Kernel` — Operating-system kernel image
  7. `Object` — Compiled object files (`.obj`, `.o`)
  8. `Package` — Distributable Robix binary package archive (`.rbxp`)
- **7 Official Compiler Profiles (`Build.Profile`):**
  - `Debug`, `Release`, `Performance`, `Size`, `RealTime`, `Embedded`, and `Safety`.
- **`Resources` Block (Asset Inclusion/Exclusion):**
  - Declarative `Include = ["Images/**", "Sounds/**", "Configs/**", "UI/**"]` and `Exclude = ["Images/Temporary/**"]` for packaging RobixUI and application assets.

##### Phase 11.4: Multi-Target Cross-Compilation & Named `Configurations`
- **Single `Target` & Multi-OS `Targets` Block:**
  - Generate platform-specific outputs (`Targets { Windows { Architecture = "x64"; OutputDirectory = "Build/Windows/"; } Linux { ... } macOS { Architecture = "arm64"; OutputDirectory = "Build/macOS/"; } }`).
  - Validate that the host has the required target SDKs, system libraries, and toolchains before building; reject invalid OS/Architecture combinations.
- **Named `Configurations` Block:**
  - Define multiple distinct builds in one `Project.rbxb` (e.g., `Desktop` -> Windows x64 `Executable` `Release`; `Server` -> Linux x64 `Executable` `Performance`; `Controller` -> `BareMetal` `ARM64` `Firmware` `Embedded`).
  - Build individually via `robix build --config Desktop`, `robix build --config Server`, `robix build --config Controller`, or all via `robix build --all`.

##### Phase 11.5: Testing & `Explain` Documentation Enforcement in `.rbxb`
- **Declarative `Testing` Policy Block:**
  - `Required = true; RequireTestFile = true; RequireTestsToPass = true;`
- **Declarative `Documentation` Policy Block:**
  - `Required = true; RequireFunctionExplain = true; RequireInputExplanation = true; RequireProcessingExplanation = true; RequireOutputExplanation = true;`
- Strictly block release builds if any required test or `Explain` section is missing or failing.
- **Security & Determinism Rules:** No arbitrary shell script execution during package builds by default; deterministic build outputs; full LSP support (autocomplete, type checking, and error diagnostics) for `.rbxb` files.
- **RobixBuild CLI Flags:** `robix build`, `--config <Name>`, `--target <OS-Arch>`, `--all`, `--clean`, `--rebuild`, `--debug`, `--release`, `--explain` (prints compilation order, DAG dependencies, and reasons), and `--dry-run` (simulates the build plan without writing outputs).

---

### STAGE 12: Robix Network — Optional Built-In Multi-Client & Multi-Server Distributed Architecture

#### Goals
Implement **Robix Network**, an optional first-class distributed application architecture orchestrated by `Project.rbxb` where multiple clients and servers share a `Common/` contract directory and invoke remote services using typed `await Server.Service.Method(...)` calls without manual HTTP/socket boilerplate—while keeping standard offline applications 100% free of networking overhead.

#### Detailed Scope & Deliverables

##### Phase 12.1: `Common/` Shared Contracts, `Service` & `Implement` Syntax
- **`Common/` Source of Truth:** Organize shared `Models/`, `Services/`, `Events/`, and `Errors/` in `Common/`.
- **`Service` Contract Declaration (`Common/UserService.rbx`):**
  ```rust
  Service UserService {
      Function GetUser(Int userId) -> User;
      Function CreateUser(String name) -> Result<User, UserError>;
      Function DeleteUser(Int userId) -> Result<Bool, UserError>;
  }
  ```
- **`Implement` Server Definition (`Server1/UserService.rbx`):**
  ```rust
  Implement UserService {
      Function GetUser(Int userId) -> User { ... }
      ...
  }
  ```
- Compiler generates typed client stubs and server dispatchers into `Build/Common/Generated/` (`NetworkContracts`, `SharedTypes`) and automatically rebuilds affected clients/servers whenever a shared contract changes.

##### Phase 12.2: Typed Asynchronous Remote Calls & Compiler Enforcement
- **Explicit Remote Call Syntax:** `let user = await Server1.UserService.GetUser(userId);`
- **Three Fundamental Design Rules:**
  1. *Local calls look local; remote calls are clearly identifiable* (`Server1.UserService.GetUser()`).
  2. *The compiler knows the difference:* Remote calls are asynchronous (`await`), strongly typed, permission-checked against `Project.rbxb`, and handle network/timeout errors (`Result<T, E>`).
  3. *`Common` defines the contract, not the implementation:* Clients and servers compile and deploy independently as long as their contract versions match.
- Behind the scenes, the Robix Network runtime automatically handles service resolution, connection checking, argument serialization, network transport, response validation, deserialization, and timeout/error reporting.

##### Phase 12.3: Multi-Client, Multi-Server Topology & Persistent Server-to-Client `Events`
- **Topology & Access-Control Boundary (`Network.Connections` in `Project.rbxb`):**
  - Explicitly declare permitted communication paths (`Client1 -> Server1; Client1 -> Server2; Server1 -> Server2; Server2 -> Server3; Server3 -> Server1;`).
  - **Compile-Time & Runtime Authorization:** Reject any attempt by a client or server to call a remote target not listed in `Network.Connections`.
- **Persistent Server-to-Client Events (NAT/Firewall-Safe):**
  - Clients initiate persistent connections (`Connect Server1;` and `Server1.Events.OnMessage(Function(message) { ... });`) over WebSockets or persistent Robix transport.
  - Servers push real-time events to connected clients (`await Client1.Events.SendTo(clientId, message);`).

##### Phase 12.4: Scoped `.env` Environment System & Secret Isolation
- **File Responsibilities:**
  - Root `.env.client` -> Applies to all Clients (`API_TIMEOUT`, `APPLICATION_NAME`, `THEME`).
  - Root `.env.server` -> Applies to all Servers (`DATABASE_HOST`, `DATABASE_PORT`, `NETWORK_TIMEOUT`).
  - `ClientN/.env` -> Applies to `ClientN` only (`SERVER1_URL`, `APPLICATION_MODE`).
  - `ServerN/.env` -> Applies to `ServerN` only (`SERVER_PORT`, `DATABASE_NAME`).
- **4-Level Resolution Priority (Highest to Lowest):**
  1. System environment variables / deployment secret manager.
  2. Application-specific `.env` (`Client1/.env` or `Server1/.env`).
  3. Root role-specific `.env.client` or `.env.server`.
  4. Explicit project-wide defaults.
- **Mandatory Security Rule:** `.env.server` and `ServerN/.env` files must **never** be copied, bundled, or exposed to client builds.

##### Phase 12.5: Enterprise Network Engine & Component Build/Run CLI
- **15 Built-In Network Engine Capabilities:**
  1. Automatic serialization
  2. Automatic deserialization
  3. Typed RPC code generation
  4. Persistent connections
  5. Real-time network events
  6. Authentication (client/service identity)
  7. Authorization (per-connection & per-function access control)
  8. Timeout handling
  9. Safe retry policies with **Request IDs and Idempotency rules** (never blindly duplicate non-idempotent operations like payments or account creation)
  10. Automatic connection recovery / reconnection
  11. Contract version checking
  12. TLS / transport encryption
  13. Load balancing across server instances
  14. Service discovery
  15. Distributed logging and request tracing across servers
- **Network Project CLI Commands:**
  - `robix new MyNetworkApp --template network`
  - `robix build --all`, `robix build --client Client1`, `robix build --server Server1`
  - `robix run --client Client1`, `robix run --server Server1`

---

### STAGE 13: Developer Experience, CLI, Tooling, Benchmarks & Documentation

#### Goals
Deliver a complete, responsive developer toolchain, C/C++/Rust benchmark suite, 22+ working example applications, and all 30+ official documentation guides.

#### Detailed Scope & Deliverables
1. **Unified Command-Line Interface (`robix` & `robixc`) — Single-File, Standard Project & Network Project Operations**
   - **Direct Native Compiler (`robixc`):**
     - Single-file compile: `robixc Main.rbx [-o Robot] [--release|--debug|--profile <name>|--target <arch>]`
     - Single-file native compile & immediate run: `robixc run Main.rbx` / `robixc run MainWindow.rbxu`
     - Single-file native test: `robixc test Main.rbx` / `robixc test Main.test.rbx`
     - Single-file UI compile: `robixc MainWindow.rbxu [-o UIApp]`
     - Project-wide build: `robixc build [--release|--debug|--target <arch>]`
   - **Unified CLI & Bytecode Interpreter (`robix`):**
     - Single-file, project, or network component run: `robix run`, `robix run Main.rbx`, `robix run MainWindow.rbxu`, `robix run --client Client1`, `robix run --server Server1`, with `--engine interpreter|native|auto` and `--watch|--debug|--trace`
     - RobixBuild commands: `robix build`, `robix build Hello.rbx --release`, `robix build --config <Name>`, `robix build --client <Name>`, `robix build --server <Name>`, `robix build --target <OS-Arch>`, `robix build --all`, `robix build --clean`, `robix build --rebuild`, `robix build --explain`, `robix build --dry-run`, `robix build --incremental`
     - Single-file or project testing: `robix test`, `robix test Main.rbx`, `robix test Main.test.rbx` (with `--engine interpreter|native`), `--all`, `--coverage`, `--repeat N`, `--memory-check`, `--stress`
     - Single-file or project verification/checking: `robix verify [File.rbx|Window.rbxu|Project.rbxb]`, `robix check [...]`, `robix format [...]` / `robix fmt [...]`, `robix lint [...] [--strict]`
     - Interactive & Developer tools: `robix repl`, `robix new <Name> [--template application|game|robotics|desktop|ai|embedded|library|network|system]`, `robix doc`, `robix debug`, `robix profile`, `robix benchmark` / `robix bench`, `robix clean`, `robix cache clear`, `robix interpreter info|version|trace`, plus all package and UI commands.
2. **IDE, Complete Debugging & Profiling Infrastructure**
   - **Language Server Protocol (LSP):** Autocomplete, go to definition, find references, rename symbols, hover `Explain` docs, and inline diagnostics across `.rbx`, `.test.rbx`, `.rbxu`, `.rbxb`, `.rbxapi`, and `.rbxproj`.
   - **VS Code Extension:** Official syntax highlighting, snippets, and task/debugger integration for all Robix file extensions.
   - **Formatter & Linter:** Enforcing PascalCase, camelCase, UPPER_SNAKE_CASE, uppercase acronyms, and `Explain` blocks.
   - **Complete Debugging Infrastructure (`robix debug` — VM & Native Parity):**
     - **Conditional Breakpoints & Watchpoints:** Source-line, function, and expression-conditioned breakpoints, plus hardware/VM memory watchpoints triggering on read/write of specific variables or addresses.
     - **Call Stack Navigation & Frame Inspection:** Full stack frame traversal, local/closure/register variable inspection, and live expression evaluation across both Bytecode VM and Native DWARF/PDB builds.
     - **Memory Inspection & Native Crash Dumps:** Raw memory/pointer hex & typed view, ownership/RC state inspection, and automatic structured crash dump generation (minidump/core dump + symbolicated Robix backtrace).
     - **VM & Native Debugging Parity:** Identical Debug Adapter Protocol (DAP) experience whether debugging in `--engine interpreter` or `--engine native`.
   - **Complete Profiling & Concurrency Diagnostics Infrastructure (`robix profile`):**
     - **Heap Visualization & Allocation Tracking:** Live object graph, arena/pool utilization, `Weak<T>` / reference cycle visualization, and per-callsite allocation hotspots.
     - **CPU Flame Graphs:** High-frequency sampling and instrumentation profilers exporting interactive SVG/JSON CPU flame graphs.
     - **Race-Condition & Deadlock Detection:** Lock-order graph analyzer for real-time deadlock detection and happens-before vector-clock race detector.
   - **Documentation Generator (`robix doc`):** Extracting `Explain` blocks, `Service` contracts, and `.rbxapi` definitions into HTML/Markdown/JSON documentation.
3. **Formal C / C++ / Rust Performance Benchmark Suite**
   - Reproducible benchmarks across 16 workloads: (1) Integer arithmetic, (2) Floating-point calculations, (3) Matrix multiplication, (4) Memory allocation, (5) Memory copying, (6) Array traversal, (7) Function calls, (8) Object creation, (9) Virtual dispatch, (10) String processing, (11) Multithreading, (12) Concurrency, (13) Robotics calculations, (14) PID control, (15) Serialization, (16) Neural network inference.
   - Metrics tracked: Execution time, memory consumption, binary size, compilation time, startup time, allocation count, peak memory, CPU utilization.
4. **Mandatory Working Example Applications (All with `.test.rbx` files)**
   1. Single-File Hello World (`Hello.rbx` — no project config required)
   2. Variables, Constants & Type Inference (`Variables.rbx`)
   3. OOP Robot Application (`Robot.rbx`, `Main.rbx`)
   4. Conditions & Battery Manager (`BatteryManager.rbx`)
   5. All Loop Styles & Motor Controller (`MotorController.rbx`)
   6. Inheritance & Polymorphism (`WalkingRobot`, `FlyingRobot`)
   7. Procedural Calculator with `Result<T, E>` & Pattern Matching (`Calculator.rbx`)
   8. Generic Storage & Collections (`Storage.rbx`)
   9. Automatic, Manual & Arena Memory Management (`MemoryExample.rbx`)
   10. Pointer Manipulation (`PointerExample.rbx`)
   11. Inline Assembly (`AssemblyExample.rbx`)
   12. Functional Programming (`Filter`/`Map`/`Reduce`)
   13. Hardware GPIO & PWM Application (`LEDController.rbx` / `Hardware.rbx`)
   14. Sensor Reader & PID Controller (`PIDController.rbx`)
   15. Autonomous Robot Application (`AutonomousRobot.rbx`)
   16. Multithreaded, Async & Multiprocessing Application (`Concurrency.rbx`)
   17. Student Manager (combining `let`, `Input<T>`, `repeat`, `List<T>`, structured `Main` return, and `Main.test.rbx`)
   18. JSON Serialization & Schema Validation (`JsonExample.rbx`)
   19. AI Matrix & Model Inference (`AIExample.rbx`)
   20. RobixUI Dashboard & Full Robot-Control Application (`MainWindow.rbxu`, `RobotDashboard.rbxu`, `MainController.rbx`)
   21. Standard Offline Game / Application (`MyGame/` with `Player.rbx`, `Enemy.rbx`, `GameWorld.rbx`, `UI/Menu.rbxu`, zero network overhead)
   22. Multi-Client, Multi-Server Distributed Application (`MyNetworkApp/` with `Project.rbxb`, `Common/UserService.rbx`, `Client1..3`, `Server1..3`, and scoped `.env` files)
5. **Complete Official Documentation Suite (30 Guides)**
   - All 28 core language/runtime/UI guides plus **(29) RobixBuild (`.rbxb`) Build Configuration Guide** and **(30) Robix Network Distributed Applications Guide**.

---

## SECTION 5: 60-POINT ROBIX SPECIFICATION & IMPLEMENTATION READINESS TRACKER

Check items off **only** when they are both formally specified and completely implemented with passing tests:

### 1. Language Fundamentals
- [x] ✅ 1. Static typing, automatic type inference (`let`, `var`, `const`, inferred returns), operator precedence specification, and Unicode identifier policy
- [x] ✅ 2. OOP, procedural, and functional programming paradigms
- [x] ✅ 3. All loop forms (`for`, `0..10`, `1..=10`, `for in`, `Enumerate`, `while`, `do-while`, `repeat`, `loop`), formal `Iterator` protocol, and pattern matching (`match`)
- [x] ✅ 4. Generics, interfaces, traits, structs, and enums
- [x] ✅ 5. Null safety (`T?`, `Option<T>`), dual error handling (`Result<T, E>`, `try/catch/finally`), and resource lifetime rules
- [x] ✅ 6. Compile-time programming (`consteval`), opt-in reflection, attributes/annotations, and macros
- [x] ✅ 7. Modules, deterministic module initialization rules, namespaces, packages, and strict visibility (`public`, `private`, `protected`, `internal`, `package`)
- [x] ✅ 8. First-class `Explain { }` function documentation blocks and naming convention enforcement (PascalCase, camelCase, UPPER_SNAKE_CASE, uppercase acronyms)

### 2. Memory and Performance
- [x] ✅ 9. Ownership, borrowing, lifetime verification, and explicit ownership transfer (`Memory.IntoRaw` / `Memory.FromRaw`)
- [x] ✅ 10. Simultaneous automatic memory management (`Weak<T>`, cycle detection, deterministic destruction order) and manual memory control (`Unsafe`, `Memory.Allocate`, `Memory.Free`, `defer`/`using`) at C-level speed
- [x] ✅ 11. Typed pointers, pointer arithmetic, references, and safe `Span<T>` slices
- [x] ✅ 12. Thread-safe custom allocators, memory pools, DMA buffers, `Arena` allocators, and configurable stack/heap limits with allocation failure handling
- [ ] 13. SIMD vector/matrix types and zero-cost abstractions
- [x] ✅ 14. Native LLVM compilation, SSA IR pass verification, Undefined Behavior rules, and build profiles (`debug`, `release`, `performance`, `realtime`, `embedded`, `safety`, `size`)
- [ ] 15. C/C++/Rust benchmarking and profiling suite

### 3. Dual Execution Engine & Single-File Mode
- [x] ✅ 16. Shared C++20/23 frontend (Lexer, Parser, AST, Type Checker, Semantic Analyzer, SSA Validated IR, crash recovery)
- [x] ✅ 17. Versioned Robix Bytecode Virtual Machine (Interpreter), `--watch` mode, and interactive `robix repl`
- [x] ✅ 18. Single-file standalone execution, compilation, UI preview, and single-file testing (`robix run/test File.rbx`, `robixc run/test File.rbx`, `robix run Window.rbxu`)
- [x] ✅ 19. Semantic conformance suite verifying identical Interpreter and Native Compiler behavior

### 4. Concurrency, OS Integration and Systems
- [ ] 20. Native threads, `Task<T>`, and thread pools
- [ ] 21. Multiprocessing (`Process`), IPC, pipes, and shared memory
- [ ] 22. `Async` / `await`, structured async cancellation semantics, and coroutines/generators
- [ ] 23. `Atomic<T>`, `Mutex`, `ReadWriteLock`, `Semaphore`, and `Channel<T>`
- [ ] 24. Real-time execution profile (`@RealTime`, `@Priority`, `@Period`, bounded non-GC execution)
- [ ] 25. Direct hardware access (`GPIO`, `PWM`, `UART`, `SPI`, `I2C`, `CAN`, `USB`, `PCIe`, `DMA`, interrupts) and architecture-checked inline `ASM`
- [x] ✅ 26. "Write Once, Compile Native Everywhere" OS integration (Windows, Linux, macOS, Android, WASM, Bare-Metal), complete Robix ABI (calling conventions, layouts, mangling, plugins), and memory-safe C FFI

### 5. Standard Library & Security Architecture
- [ ] 27. Built-in `Print`, `Input<T>`, rich numeric/string/boolean/date-time types, first-class linear-time `Robix.Regex`, and `Robix.Collections`
- [ ] 28. First-class `Robix.Json` (parse, typed serialize/deserialize, streaming, schema validation), `Xml`, `Csv`, `Toml`, `Yaml`
- [ ] 29. `Robix.IO` cross-platform files/streams and `Robix.Network` (TCP, UDP, HTTP, WebSockets, MQTT, gRPC)
- [ ] 30. Formal `Robix.Security` architecture (threat model, sandboxed execution, `Secret<T>` storage, supply-chain/vulnerability scanning, cryptography, CSPRNG, capability permissions `requires ...`)
- [ ] 31. `Robix.AI`, `Robix.GPU`, `Matrix<T>`, `Tensor<T>`, and scientific computing
- [ ] 32. `Robix.Robotics`, sensors, actuators, motors, PID control, kinematics, and navigation
- [ ] 33. `Robix.Database` (SQLite/drivers), `Robix.Compression`, and binary serialization

### 6. RobixUI (`.rbxu`)
- [ ] 34. `.rbxu` XML-inspired parser, styling, and resource support
- [ ] 35. Full UI component library (`Window`, `Column`, `Row`, `Grid`, `Button`, `Text`, `Chart`, `Canvas`, etc.) and layout engine
- [ ] 36. Type-checked one-way/two-way data binding, event binding (`OnClick`, etc.), and pluggable rendering runtime

### 7. RobixBuild (`.rbxb`) & Project Modes
- [ ] 37. Declarative, statically typed `.rbxb` parser & schema with zero-config fallback when `.rbxb` is omitted
- [ ] 38. Automatic `import` dependency DAG, circular dependency detection, and explicit `Order` (`First`, `Before`, `Last`)
- [ ] 39. All 8 output types (`Executable`, `Library`, `SharedLibrary`, `StaticLibrary`, `Firmware`, `Kernel`, `Object`, `Package`) and `Resources` (`Include`/`Exclude`)
- [ ] 40. Multi-OS `Targets` block, named `Configurations` (`--config`), `--explain`, and `--dry-run`
- [ ] 41. All 8 official project styles (`SingleFile`, `Application`, `Game`, `Library`, `Embedded`, `System`, `Network`, `Custom`)

### 8. Robix Network (Optional Distributed Architecture)
- [ ] 42. `Common/` shared contracts, `Service` declarations, and `Implement` server definitions
- [ ] 43. Typed asynchronous remote calls (`await Server1.UserService.GetUser()`) with automatic serialization/deserialization
- [ ] 44. `Network.Connections` topology authorization and NAT-safe persistent server-to-client `Events` (`Connect Server1`, `Events.SendTo`)
- [ ] 45. Scoped `.env` loading (`.env.client`, `.env.server`, `ClientN/.env`, `ServerN/.env`) with strict server secret isolation
- [ ] 46. Idempotent retry policies (Request IDs), timeouts, reconnection, contract version checks, TLS, service discovery, and distributed tracing

### 9. Packages and Distribution
- [ ] 47. `.rbxapi` public API definitions with mandatory `Explain` blocks
- [ ] 48. `.rbxp` compiled binary package archives with target-specific binaries and SBOM
- [ ] 49. Project-local package installation (`.robix/packages/`) via `robix`
- [ ] 50. `.rbxproj` manifest and deterministic `.rbxlock` dependency lockfile
- [ ] 51. Cryptographic package signatures, SHA-256 verification, and ABI compatibility checks
- [ ] 52. Package registry client/server workflows (`search`, `publish`, `audit`, `verify-package`)

### 10. Testing and Tooling
- [ ] 53. Mandatory per-file `.test.rbx` convention and build blocker (`R5001`)
- [ ] 54. Unit, integration, boundary, negative, and hardware simulation testing (`Robix.Testing`)
- [ ] 55. Fuzzing, property-based testing, and mutation testing
- [ ] 56. Single-command project/file verifier (`robix verify`), formatter (`robix fmt`), and linter (`robix lint --strict`)
- [ ] 57. Complete debugging & profiling infrastructure (`robix debug` / `robix profile`: conditional breakpoints, watchpoints, stack navigation, memory inspection, heap visualization, CPU flame graphs, race/deadlock detection, crash dumps, VM & native parity)
- [ ] 58. Language Server Protocol (LSP) and VS Code extension (including `.rbxb` and `.rbxu` support)

### 11. Platform and Governance
- [ ] 59. Cross-compilation across Windows, Linux, macOS, Android, Embedded, and Bare-Metal (`x86_64`, `ARM64`, `ARM32`, `RISC-V`, `WASM32`)
- [ ] 60. Complete EBNF formal language specification, 30-part official documentation suite, deterministic incremental builds, `LanguageVersion` compatibility guarantees, and all 22 working example applications

