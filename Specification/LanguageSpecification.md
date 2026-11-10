# ROBIX FORMAL LANGUAGE SPECIFICATION (v1.0)
## A Complete Systems, Robotics & AI Programming Language by Roboticela

**Language Version:** `1.0`  
**Formal Grammar References:** [`RobixGrammar.ebnf`](file:///d:/NEW/Robix/Specification/RobixGrammar.ebnf), [`RobixUIGrammar.ebnf`](file:///d:/NEW/Robix/Specification/RobixUIGrammar.ebnf)

---

## 1. LEXICAL STRUCTURE & UNICODE IDENTIFIER POLICY

### 1.1 Source Encoding
All Robix source files (`.rx`, `.test.rx`, `.rbxapi`, `.rbxb`, `.rbxproj`, `.rxu`) are encoded in **UTF-8** without requiring a Byte Order Mark (BOM).

### 1.2 Unicode Identifier Policy (UAX #31)
1. Identifiers follow **Unicode Standard Annex #31 (UAX #31)** `XID_Start` and `XID_Continue` properties, normalized to **Unicode Normalization Form C (NFC)** prior to symbol resolution.
2. Two identifiers are considered identical if and only if their NFC-normalized UTF-8 byte sequences are identical.
3. Confusable characters or bidirectional override control characters (`U+202A`..`U+202E`, `U+2066`..`U+2069`) inside identifiers or comments are rejected at lexing time to prevent Trojan Source attacks.

### 1.3 Official Naming & Capitalization Enforcement
The compiler (`robixc`) and linter (`robix lint --strict`) enforce:
- **`PascalCase`** for filenames (`RobotController.rx`), classes, structs, interfaces, traits, enums, enum variants, functions, methods, constructors, services, and package names.
- **`camelCase`** for local variables (`motorSpeed`), instance fields, parameters, and local bindings.
- **`UPPER_SNAKE_CASE`** for all compile-time constants (`MAX_SPEED`, `PI_VALUE`).
- **Acronym Preservation:** Acronyms (`SDK`, `XML`, `CPP`, `API`, `HTTP`, `GPU`, `CPU`, `USB`, `AI`, `JSON`, `TCP`, `UART`, `PWM`, `GPIO`, `I2C`, `SPI`, `CAN`, `DMA`) remain uppercase (`APIManager`, `HTTPClient`, `isUSBConnected`, `apiURL`).

---

## 2. OPERATOR PRECEDENCE & ASSOCIATIVITY SPECIFICATION

Operators are ordered from **highest precedence (18)** to **lowest precedence (1)**:

| Precedence | Category | Operators | Associativity |
| :---: | :--- | :--- | :--- |
| **18** | Primary | Literals, `this`, `Base`, `(expr)`, `[a, b]`, `{ Key: val }` | — |
| **17** | Postfix & Member | `x.y`, `x?.y`, `f(x)`, `a[i]`, `x++`, `x--`, `expr?` (error try) | Left-to-Right |
| **16** | Unary Prefix | `!x`, `-x`, `+x`, `~x`, `*ptr`, `&x`, `&mut x`, `++x`, `--x`, `await x`, `new T()` | Right-to-Left |
| **15** | Multiplicative | `*`, `/`, `%` | Left-to-Right |
| **14** | Additive | `+`, `-` | Left-to-Right |
| **13** | Bitwise Shift | `<<`, `>>` | Left-to-Right |
| **12** | Range | `start..end` (exclusive), `start..=end` (inclusive) | Non-Associative |
| **11** | Relational & Type | `<`, `<=`, `>`, `>=`, `is`, `as` | Left-to-Right |
| **10** | Equality | `==`, `!=` | Left-to-Right |
| **9** | Bitwise AND | `&` | Left-to-Right |
| **8** | Bitwise XOR | `^` | Left-to-Right |
| **7** | Bitwise OR | `\|` | Left-to-Right |
| **6** | Logical AND | `&&` (short-circuiting) | Left-to-Right |
| **5** | Logical OR | `\|\|` (short-circuiting) | Left-to-Right |
| **4** | Null-Coalescing | `a ?? b` | Left-to-Right |
| **3** | Ternary Conditional | `cond ? thenExpr : elseExpr` | Right-to-Left |
| **2** | Lambda / Closure | `x => expr`, `(a, b) => { ... }` | Right-to-Left |
| **1** | Assignment | `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `\|=`, `^=`, `<<=`, `>>=` | Right-to-Left |

### 2.1 Evaluation Order
Function arguments, binary operator operands, array elements, and struct initializers are strictly evaluated **left-to-right** in both the Bytecode Interpreter and the Native Compiler.

---

## 3. STATIC TYPE SYSTEM & PRIMITIVE LAYOUT SPECIFICATION

### 3.1 Exact Primitive Type Sizes, Signedness, Ranges & Alignment
No primitive type has an ambiguous size:

| Type | Size (Bytes) | Alignment | Signed? | Exact Value Range / Representation |
| :--- | :---: | :---: | :---: | :--- |
| `Bool` | 1 | 1 | No | `false` (`0x00`) or `true` (`0x01`) |
| `Byte` / `UInt8` | 1 | 1 | No | $0$ to $255$ |
| `Int8` | 1 | 1 | Yes | $-128$ to $127$ (Two's complement) |
| `Int16` | 2 | 2 | Yes | $-32,768$ to $32,767$ |
| `UInt16` | 2 | 2 | No | $0$ to $65,535$ |
| `Int32` | 4 | 4 | Yes | $-2^{31}$ to $2^{31}-1$ |
| `UInt32` | 4 | 4 | No | $0$ to $2^{32}-1$ |
| `Int64` | 8 | 8 | Yes | $-2^{63}$ to $2^{63}-1$ |
| `UInt64` | 8 | 8 | No | $0$ to $2^{64}-1$ |
| `Int` | 8 (64-bit) / 4 (32-bit) | Pointer | Yes | Platform pointer-width signed integer (`ssize_t` / `intptr_t`) |
| `UInt` | 8 (64-bit) / 4 (32-bit) | Pointer | No | Platform pointer-width unsigned integer (`size_t` / `uintptr_t`) |
| `Float16` | 2 | 2 | Yes | IEEE-754 binary16 half-precision |
| `Float32` | 4 | 4 | Yes | IEEE-754 binary32 single-precision |
| `Float64` / `Float` / `Double` | 8 | 8 | Yes | IEEE-754 binary64 double-precision (`Float` defaults to 64-bit) |
| `Decimal` | 16 | 16 | Yes | 128-bit high-precision decimal arithmetic |
| `Char` | 4 | 4 | No | Unicode Scalar Value (`U+0000`..`U+D7FF`, `U+E000`..`U+10FFFF`) |
| `String` | 24 (64-bit) | 8 | — | UTF-8 fat representation (`ptr`, `length`, `capacity` + Small String Optimization) |
| `Void` | 0 | 1 | — | Unit / no value |

### 3.2 Integer Overflow & Floating-Point Semantics
- **Integer Overflow:**
  - In `Debug` and `Safety` profiles: Signed and unsigned arithmetic overflow traps deterministically with a structured diagnostic (`Error R2005: Integer overflow`).
  - In `Release`, `Performance`, `RealTime`, and `Embedded` profiles: Defined two's-complement wrapping (never C-style Undefined Behavior), with explicit `.CheckedAdd()`, `.WrappingAdd()`, and `.SaturatingAdd()` methods always available on all integer types.
- **Floating-Point & Trigonometry:**
  - Conforms to IEEE-754. Trigonometric functions in `Robix.Math` (`Math.Sin`, `Math.Cos`, `Math.Tan`) take **radians** by default, with explicit `Math.DegreesToRadians()` / `Math.SinDegrees()` helpers provided.

### 3.3 Type Safety & Inference (`let`, `var`, `const`)
- **No Implicit Unrelated or Narrowing Conversions:** `Int + Float` is rejected (`Error R1001`); requires explicit `speed.ToFloat() + temperature`.
- **`let`:** Immutable statically inferred binding.
- **`var`:** Mutable statically inferred binding; cannot change static type after initialization.
- **`const`:** Compile-time evaluated constant (`UPPER_SNAKE_CASE`).
- **Null Safety:** Ordinary types (`String`, `Robot`) are never `null`. Nullable types require `T?` (`String? name = null;`) or `Option<T>` and must be checked (`if name != null` or `match`) before dereferencing.

---

## 4. LOOP FORMS, ITERATOR PROTOCOL & CONTROL FLOW

### 4.1 Formal Iterator Protocol (`Iterable<T>` & `Iterator<T>`)
Every collection in `Robix.Collections` and any user type implementing `Iterable<T>` adheres to:
```rust
Public Trait Iterator<T> {
    Function Next() -> Option<T>;
}

Public Trait Iterable<T> {
    Function GetIterator() -> Iterator<T>;
}
```
The compiler lowers `for item in collection` into stack-inlined `GetIterator()` and `Next()` calls with **zero heap allocation or virtual call overhead**.

### 4.2 All 10 Official Loop Forms
1. **Traditional `for`:** `for (Int i = 0; i < 10; i++) { ... }`
2. **Exclusive range:** `for i in 0..10 { ... }` ($0$ through $9$)
3. **Inclusive range:** `for i in 1..=10 { ... }` ($1$ through $10$)
4. **Collection iteration:** `for item in collection { ... }`
5. **Enumerated iteration:** `for (index, item) in collection.Enumerate() { ... }`
6. **`while` loop:** `while condition { ... }`
7. **`do-while` loop:** `do { ... } while condition;`
8. **`repeat` loop:** `repeat N { ... }` (executes $N$ times; if $N \le 0$, executes $0$ times)
9. **`loop` (infinite loop):** `loop { ... }` (primary loop for embedded/OS/robotics control loops)
10. **Control transfer:** `break;`, `continue;`, `return expr;`

---

## 5. MODULE INITIALIZATION, VISIBILITY & FLEXIBLE `Main()`

### 5.1 Deterministic Module Initialization Rules
1. Top-level `const` declarations are evaluated at compile time.
2. Module-level static variables are initialized in topological dependency order of the `import` Directed Acyclic Graph (DAG) before `Main()` begins.
3. Cyclic initialization dependencies between static variables across modules are rejected at compile time.

### 5.2 Access & Package Visibility Rules
| Modifier | Same Class/Struct | Derived Class | Same File/Module | Same Package | External Consumer (`.rbxapi`) |
| :--- | :---: | :---: | :---: | :---: | :---: |
| `private` | Yes | No | No | No | No |
| `protected` | Yes | Yes | No | No | No |
| `package` | Yes | Yes (in pkg) | Yes | Yes | No |
| `internal` | Yes | Yes (in build) | Yes | Yes | No (same `.rbxb` target only) |
| `public` | Yes | Yes | Yes | Yes | Yes |

### 5.3 Flexible `Main()` Entry Point Contract
| `Main()` Signature | Runtime & OS Process Exit Behavior |
| :--- | :--- |
| `Main()` / `Main() -> Void` | Executes body; exits process with OS exit code `0`. |
| `Main() -> Int` | Returned `Int` becomes the OS process exit code (`0` = success, non-zero = error). |
| `Main() -> String` / `Array<T>` / `Object` | Prints/serializes structured output via runtime launcher; exits with code `0`. |
| `Main() -> Result<T, E>` | On `Success(v)`, outputs `v` (if non-`Void`) and exits `0`; on `Error(e)`, prints diagnostic to `stderr` and exits with code `1` (or error code). |

---

## 6. FIRST-CLASS `Explain { }` DOCUMENTATION SPECIFICATION

Every function or method supports a structured `Explain` block between the signature and body:
- **`Description`** *(Required)*: String literal explaining the purpose of the function.
- **`Input`** *(Required if function has parameters)*: Explains every parameter by exact name (`param: "meaning"` or compact `a = "...", b = "..."`). Compiler verifies parameter names match the function signature.
- **`Processing`** *(Required)*: Explains the algorithm/processing performed.
- **`Output`** *(Required if return type is not `Void`)*: Explains the returned value.
- **`Errors`** *(Required if returning `Result` or throwing)*: Documents each error variant/condition.
- **`SideEffects`** *(Required if mutating external state, I/O, or hardware)*: Documents side effects.
- Missing required fields trigger `Warning D3001` (or build error when `Documentation { Required = true; }` is set in `Project.rbxb` or inside `.rbxapi`).

---

## 7. UNIFIED MEMORY MODEL, RESOURCE LIFETIMES & UNDEFINED BEHAVIOR

### 7.1 Zero Undefined Behavior in Safe Code
Safe Robix code (outside `Unsafe { ... }`) has **zero Undefined Behavior**:
- All variables must pass definite-assignment analysis before read.
- Array/slice indexing is bounds-checked (unless statically proven in-bounds by the optimizer).
- Null dereferences are prevented statically by `T?` / `Option<T>`.

### 7.2 Coexistence of Automatic & Manual Memory at C-Level Speed
1. **Stack & Value Types (`Struct`):** Allocated on stack; copied/moved by value; zero heap cost.
2. **Single-Owner & Escape-Analyzed Objects:** Objects that do not escape a function are stack-allocated; single-owner heap objects are freed deterministically at scope exit (`O(1)`, no GC pause).
3. **Shared Automatic Objects & `Weak<T>`:** Thread-local non-atomic reference counting (or atomic RC for cross-thread objects) with `Weak<T>` weak references and cycle detection to prevent leaks.
4. **Scoped Regions (`Arena`, `Pool`, `DMA`):** `using arena { ... }` performs $O(1)$ bump allocation; compiler borrow/lifetime checker forbids arena references from escaping the `using` block.
5. **Manual `Unsafe` Memory & Ownership Transfer:**
   - `Memory.Allocate<T>(n)` and `Memory.Free(ptr)` inside `Unsafe { ... }`.
   - `Memory.IntoRaw(obj) -> Pointer<T>` and `Memory.FromRaw(ptr) -> T` provide explicit, audited ownership transfer between automatic and manual realms.
6. **Deterministic Destruction Order:** Local variables and `defer { ... }` blocks execute in **exact reverse order of declaration** on scope exit, early `return`, `Result` error return, exception unwinding, or async task cancellation.

### 7.3 Resource Lifetime Rules & Stack/Heap Budgets
- Hardware peripherals (`GPIO`, `PWM`, `UART`, `SPI`, `I2C`, `CAN`, `DMA`), files, and sockets follow linear RAII ownership (`using` / `defer`) so two tasks cannot conflict on the same physical pin/channel without explicit synchronization.
- Embedded and safety-critical builds can set compile-time and runtime `MaxStackSize` and `MaxHeapSize` limits in `Project.rbxb`, using `Memory.TryAllocate<T>()` to handle out-of-memory without panicking.

---

## 8. CONCURRENCY, ASYNC CANCELLATION & REAL-TIME SEMANTICS

1. **Structured Async Cancellation:** Every `Async Function` and `Task<T>` participates in structured concurrency via `CancellationToken`. When a parent task or timeout scope cancels, child tasks are signalled at their next `await` point and deterministically execute all active `defer` and `using` cleanup blocks before terminating.
2. **Data-Race Prevention:** Mutable state cannot be shared across `Thread` or `Task` boundaries unless wrapped in `Atomic<T>`, `Mutex<T>`, `ReadWriteLock<T>`, or transferred via `Channel<T>`.
3. **Real-Time Profile (`@RealTime`, `@Priority`, `@Period`):** Statically forbids unbounded heap/GC operations, dynamic loading, and non-priority-inheriting locks inside `@RealTime` functions.

---

## 9. "WRITE ONCE, COMPILE NATIVE EVERYWHERE" OS INTEGRATION

Developers write OS-independent Robix code once (`File`, `Thread`, `Process`, `Network`, `Robix.UI`, `Hardware`). During compilation, the Robix compiler monomorphizes and inlines standard library calls directly to target-native OS primitives:
- **Windows:** Win32 / NT kernel APIs, IOCP, DirectX / Win32 windowing.
- **Linux:** Direct syscalls / `libc`, `io_uring` / `epoll`, `futex`, Wayland / X11 / DRM, `sysfs` / `ioctl`.
- **macOS:** Darwin / Mach syscalls, `kqueue` / GCD, Cocoa / Metal.
- **Android:** Android NDK / Bionic, NativeActivity, Vulkan / GLES.
- **WebAssembly:** `wasm32-wasi` and linear memory intrinsics.
- **Bare-Metal:** Direct CPU register, interrupt vector, and MMIO generation.
This guarantees **zero wrapper or virtual-dispatch overhead** while preserving 100% source portability.

