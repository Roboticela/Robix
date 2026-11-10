# ROBIX APPLICATION BINARY INTERFACE (ABI) & GOVERNANCE SPECIFICATION (v1.0)
## Binary Compatibility, Calling Conventions, Memory Layouts & Language Governance

**ABI Version:** `RobixABI_v1`  
**Language Version:** `1.0`  
**Product of Roboticela**

---

## 1. ROBIX CALLING CONVENTIONS

Robix uses a zero-overhead native calling convention (`robixcc`) that aligns with the platform's C ABI for register parameters while adding dedicated support for `Result<T, E>` returns and `this` pointers:

| Target Architecture | Integer / Pointer Args | Floating-Point / SIMD Args | Return Value Registers | Stack Alignment |
| :--- | :--- | :--- | :--- | :---: |
| **`x86_64` (System V: Linux / macOS)** | `RDI`, `RSI`, `RDX`, `RCX`, `R8`, `R9` | `XMM0` .. `XMM7` | `RAX`, `RDX` / `XMM0`, `XMM1` | 16-byte |
| **`x86_64` (Windows x64)** | `RCX`, `RDX`, `R8`, `R9` | `XMM0` .. `XMM3` | `RAX` / `XMM0` | 16-byte |
| **`ARM64` (`aarch64`)** | `X0` .. `X7` (`X8` indirect result) | `V0` .. `V7` | `X0`, `X1` / `V0`, `V1` | 16-byte |
| **`ARM32` (`eabihf`)** | `R0` .. `R3` | `S0` .. `S15` / `D0` .. `D7` | `R0`, `R1` / `S0`, `D0` | 8-byte |
| **`RISC-V` (`riscv64gc`)** | `a0` .. `a7` | `fa0` .. `fa7` | `a0`, `a1` / `fa0`, `fa1` | 16-byte |
| **`WASM32` (`wasm32`)** | Virtual typed locals | Virtual typed locals | Single/multi-value stack | 16-byte |

### 1.1 `this` Pointer & Method Calls
- For instance methods on `Class` and `Struct`, the implicit `this` pointer is always passed as the **first pointer argument** (`RDI` on System V x64, `RCX` on Windows x64, `X0` on ARM64).
- Static methods and top-level procedural functions have no implicit `this` parameter and are 100% binary-compatible with standard C function pointers when declared `Extern` or `@ExportC`.

---

## 2. OBJECT, STRUCT & COLLECTION MEMORY LAYOUTS

### 2.1 Value Structs (`Struct`)
- Fields are laid out sequentially in declaration order using standard natural alignment padding (matching C struct layout by default so structs can pass across C FFI with zero conversion).
- **`@Packed` Attribute:** Removes all inter-field padding (alignment = 1 byte).
- **`@Align(N)` Attribute:** Forces minimum byte alignment $N$ (where $N$ is a power of 2, e.g., `@Align(64)` for cache-line or DMA alignment).
- **Zero Vtable Overhead:** `Struct` instances never contain a hidden vtable header.

### 2.2 Classes (`Class`)
Heap or stack-promoted `Class` instances use a deterministic 16-byte header (on 64-bit targets) followed by instance fields:
```text
+-------------------------------------------------------------------+
| Offset 0x00 (8 bytes): VTable / TypeMetadata Pointer              |
| Offset 0x08 (8 bytes): Reference Count & Ownership Flags Header   |
+-------------------------------------------------------------------+
| Offset 0x10+         : Base Class Fields (in declaration order)   |
| ...                  : Derived Class Fields (in declaration order)|
+-------------------------------------------------------------------+
```
- Note: For classes that have no virtual methods (`final` classes without dynamic interface dispatch) and are statically proven single-owner or stack-promoted, the compiler elides unused header words under the `performance` and `embedded` profiles.

### 2.3 Slices (`Span<T>`), `String` & `Result<T, E>` Layouts
- **`Span<T>` (16 bytes on 64-bit):** `{ Pointer<T> data; UInt length; }` — passed directly in two registers (`RDI` + `RSI` / `X0` + `X1`) with zero heap allocation.
- **`String` (24 bytes on 64-bit):** `{ Pointer<UInt8> utf8Data; UInt byteLength; UInt capacityAndFlags; }` (supports inline Small String Optimization up to 23 bytes without heap allocation).
- **`Result<T, E>`:** Tagged union `{ UInt8 tag; /* 0 = Success, 1 = Error */ union { T value; E error; } }`. When `T` or `E` fits in a register, `Result<T, E>` is returned entirely in two CPU registers (`RAX` = tag, `RDX` = payload) without touching memory or unwinding tables.

---

## 3. CANONICAL NAME MANGLING SPECIFICATION

All non-`Extern` Robix symbols use the canonical `_RX` prefix (ABI v1: `_RX1_`):
```text
_RX1_<PackageLength><Package>_<ClassOrModuleLength><ClassOrModule>_<FuncLength><Func>_<ParamTypeCodes>__<ReturnTypeCode>
```
### Type Codes
- `v` = `Void`, `b` = `Bool`, `c` = `Char`, `s` = `String`
- `i8`, `i16`, `i32`, `i64`, `is` (`Int`)
- `u8`, `u16`, `u32`, `u64`, `us` (`UInt`)
- `f16`, `f32`, `f64`, `d` (`Decimal`)
- `P<Type>` = `Pointer<Type>`, `R<Type>` = `&Type`, `M<Type>` = `&mut Type`, `S<Type>` = `Span<Type>`
- `G<Count>_<Type1>_<Type2>` = Generic instantiation

**Example:**
- `package Robotics.Motors; public class Motor { public Void SetSpeed(Int value) }`
- Mangled symbol: `_RX1_8Robotics6Motors_5Motor_8SetSpeed_is__v`

---

## 4. GENERIC ABI, `.rxapi` & `.rxp` BINARY COMPATIBILITY

1. **Monomorphization & Pre-Instantiation:** Generic classes and functions are monomorphized into specialized native machine code for each concrete type.
2. **Cross-Package Generics in `.rxp`:** Published `.rxp` packages include both pre-compiled native binaries for exported concrete types and validated IR templates inside the package archive so consuming applications can instantiate generic types (`List<MyCustomStruct>`) with full native optimization.
3. **ABI Verification Hash:** Every `.rxp` package embeds a `SHA-256` hash of its `.rxapi` declarations, target triple (`windows-x64`, `linux-x64`, `macos-arm64`, etc.), and `ABI = "RobixABI_v1"`. The linker refuses to link mismatched ABI versions and emits a clear diagnostic.
4. **Plugin & Dynamic Library ABI:** Shared libraries (`Output = "SharedLibrary"` in `Project.rxb`) expose a standard `_RX1_PluginMetadata` descriptor table verifying compiler ABI compatibility upon dynamic load.

---

## 5. LANGUAGE VERSIONING, COMPATIBILITY & GOVERNANCE POLICY

### 5.1 Explicit Language Versioning
Every `Project.rxproj` / `Project.rxb` declares:
```rust
LanguageVersion = "1.0";
```
1. **Zero Silent Breaking Changes:** A compiler upgrade (e.g., `robixc` 1.1, 1.2) must **never** silently alter the runtime semantics of existing `LanguageVersion = "1.0"` programs.
2. **Strict Deprecation Lifecycle:** Any standard library API or syntax scheduled for change must first be marked `@Deprecated("Reason and replacement", Since: "1.x")` for at least one major cycle, with automated migration provided via `robix fmt --migrate`.
3. **Public Robix Enhancement Proposal (REP) Process:** Any new grammar construct, keyword, or ABI modification requires:
   - Formal EBNF grammar update in [`RobixGrammar.ebnf`](file:///d:/NEW/Robix/Specification/RobixGrammar.ebnf).
   - Dual-engine semantic specification (Interpreter + Native LLVM parity).
   - Conformance and negative compilation test cases before implementation is marked complete.

