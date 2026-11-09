# Robix language features

Language version `1.0`. Company: Roboticela. Tagline: "The Language of Intelligent Machines."

This file lists the syntax, types, declarations, statements, operators, built-in calls, standard-library packages, and RobixUI markup that the language defines. The formal grammars are `Specification/RobixGrammar.ebnf` and `Specification/RobixUIGrammar.ebnf`. The keyword list is the lexer in `Compiler/Lexer/Lexer.cpp`. Callable names are the type checker, the native backend, and the files under `StdLib/`. What the current `robixc` and interpreter have actually executed is recorded in `Documentation/Status.md`.

Source files are UTF-8. Identifiers follow Unicode UAX #31 (`XID_Start` / `XID_Continue`), compared after NFC normalization. Bidirectional override characters inside identifiers or comments are rejected.

## 1. Files and tools

| Kind | Extension or name |
| :--- | :--- |
| Source | `.rbx` |
| Interface | `.rbxh` |
| Test source | `.test.rbx` |
| API description | `.rbxapi` |
| Compiled module | `.rbxm` |
| Library | `.rbxlib` |
| Package metadata | `.rbxp` |
| Project | `.rbxproj` |
| Workspace | `.rbxworkspace` |
| Build description | `.rbxb` |
| Lockfile | `Robix.Lock` |
| UI document | `.rbxu` |
| UI theme | `.rbxtheme` |
| UI assets | `.rbxassets` |
| Compiler | `robixc` |
| Package manager and tools | `robix` |

Commands the tools accept:

```text
robixc File.rbx -o App -O3
robixc --target <triple> -O3 File.rbx
robixc --version
robixc --help
robixc --emit-llvm
robixc --keep-ir
robixc --explain
robixc --dry-run
robixc --no-main
robixc --profile debug|release|performance|realtime|safety|size|embedded
robixc --entry <symbol>

robix run [File.rbx] [--engine interpreter|native|auto] [--native] [--watch] [--debug] [--trace]
robix run all [--separate]
robix repl
robix new [.] [Name] [Application|Canvas|Bedrock] [kind]
robix new file <Name> <class|package|empty>
robix build [--debug|--release] [--incremental] [--profile <name>] [--target <triple>]
robix check
robix test
robix format [--check]
robix lint [--strict]
robix doc
robix debug [--attach]
robix lsp
robix clean
robix add
robix remove
robix update
robix install
robix restore
robix ui validate
robix ui preview
robix ui render
robix ui window [--dev]
robix ui web [--dev]
robix network check
robix network run
robix network probe
robix interpreter info
robix interpreter version
robix interpreter trace
```

Target triples the compiler accepts: `x86_64` hosted Windows, `x86_64-unknown-none`, `aarch64-unknown-none`, `arm-unknown-none`, `riscv64-unknown-none`, `x86_64-unknown-linux-gnu`. A project `Output` of `Kernel` or `Firmware` links a boot image. Other outputs are `Executable`, a static library, a shared library, an object file, or a package (`.rbxlib`, header `RBXPKG 1`).

Build profiles select the link flags recorded in `Documentation/Status.md`: `debug`, `release` (default `-O3`), `performance`, `realtime`, `safety`, `size`, `embedded`.

## 2. Naming

| Kind | Case | Examples |
| :--- | :--- | :--- |
| Files, packages, classes, structs, interfaces, traits, enums, variants, functions, methods, constructors, services | PascalCase | `RobotController`, `APIManager`, `HTTPClient` |
| Locals, fields, parameters, properties | camelCase | `motorSpeed`, `isUSBConnected`, `apiURL` |
| Constants | UPPER_SNAKE_CASE | `MAX_SPEED`, `PI_VALUE` |

Acronyms stay uppercase: `API`, `HTTP`, `GPU`, `CPU`, `USB`, `AI`, `XML`, `JSON`, `TCP`, `UART`, `PWM`, `GPIO`, `I2C`, `SPI`, `CAN`, `DMA`. The compiler does not rename identifiers. `robix lint --strict` rejects a constant that is not `UPPER_SNAKE_CASE` (`L0004`) and a public function that has no `Explain` block (`D3001`).

Comments:

```robix
// line comment
/* block comment */
```

## 3. Keywords

The lexer accepts both the lowercase and the PascalCase spelling where both are listed.

Modules and visibility: `package`, `Package`, `import`, `Import`, `export`, `Export`, `as`, `public`, `Public`, `private`, `Private`, `protected`, `Protected`, `internal`, `Internal`.

Types and declarations: `class`, `Class`, `abstract`, `Abstract`, `final`, `Final`, `partial`, `Partial`, `struct`, `Struct`, `enum`, `Enum`, `interface`, `Interface`, `trait`, `Trait`, `extends`, `implements`, `override`, `static`, `operator`, `function`, `Function`, `consteval`, `async`, `Async`, `await`, `Constructor`, `Destructor`, `Base`, `this`, `new`, `type`, `where`, `requires`, `is`.

Bindings: `let`, `var`, `const`.

Documentation fields: `Explain`, `Description`, `Input`, `Processing`, `Output`, `Errors`, `SideEffects`.

Control: `if`, `else`, `match`, `switch`, `case`, `default`, `for`, `in`, `while`, `do`, `repeat`, `loop`, `break`, `continue`, `return`, `yield`, `goto`.

Errors and systems: `try`, `catch`, `finally`, `throw`, `defer`, `using`, `Unsafe`, `ASM`, `extern`, `Extern`.

Services, tests, macros: `Service`, `Implement`, `Connect`, `Permissions`, `Test`, `test`, `TestFile`, `Covers`, `macro`.

Literals that are keywords: `true`, `false`, `null`.

`switch`, `case`, `default`, `goto`, `yield`, and `export` are reserved words. The statement parser lowers `if`, `for`, `while`, `do`, `repeat`, `loop`, `match`, `try`, `return`, `break`, `continue`, `defer`, `using`, `Unsafe`, `ASM`, `throw`, and `Connect`. It does not lower `switch`, `goto`, or `yield`.

The grammar also writes `package` as an access modifier. The lexer treats `package` as the package keyword, so visibility in source is `public`, `private`, `protected`, and `internal`.

## 4. Literals

```robix
42
1_000
0xFFF
0b1010
0o755
42i8
42i16
42i32
42i64
42u8
42u16
42u32
42u64

3.14
1.5e-3
1.0f16
1.0f32
1.0f64
1.0d

true
false
null
'A'
"Hello"
"""
line
line
"""
$"Robot {name} speed is {speed}"
r"[0-9]+"
```

Integer suffixes: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`.

Float suffixes: `f16`, `f32`, `f64`, `d`.

String escapes: `\n`, `\r`, `\t`, `\0`, `\\`, `\"`, `\'`, `\{`, `\}`, `\xHH`, `\u{hex}`.

A fitting integer literal may initialize a narrower integer type.

## 5. Primitive types

| Type | Size | Signed | Range or representation |
| :--- | :--- | :--- | :--- |
| `Bool` | 1 | no | `false` (`0x00`) or `true` (`0x01`) |
| `Byte` | 1 | no | alias of `UInt8`, 0 to 255 |
| `UInt8` | 1 | no | 0 to 255 |
| `Int8` | 1 | yes | −128 to 127 |
| `Int16` | 2 | yes | −32768 to 32767 |
| `UInt16` | 2 | no | 0 to 65535 |
| `Int32` | 4 | yes | −2^31 to 2^31−1 |
| `UInt32` | 4 | no | 0 to 2^32−1 |
| `Int64` | 8 | yes | −2^63 to 2^63−1 |
| `UInt64` | 8 | no | 0 to 2^64−1 |
| `Int` | pointer width | yes | 8 bytes on 64-bit targets |
| `UInt` | pointer width | no | 8 bytes on 64-bit targets |
| `Float16` | 2 | yes | IEEE-754 binary16 |
| `Float32` | 4 | yes | IEEE-754 binary32 |
| `Float` | 8 | yes | IEEE-754 binary64. `Float` is 64-bit |
| `Float64` | 8 | yes | IEEE-754 binary64 |
| `Double` | 8 | yes | same as `Float64` |
| `Decimal` | 16 | yes | 128-bit decimal |
| `Char` | 4 | no | Unicode scalar value |
| `String` | 24 on 64-bit | — | UTF-8 |
| `Void` | 0 | — | no value |

Release and performance integer arithmetic wraps. It is defined two's-complement wrap, not a signed-overflow trap. Signed `>>` is an arithmetic shift. `<<` and `>>` lower to LLVM `shl` and `ashr`. A positive power-of-two divisor lowers to a shift or a mask. Floating-point arithmetic in a release build is marked `fast`. Trigonometric functions take radians.

`Int` and `Float` do not add. The conversion is explicit: `value.ToFloat()` or `value.ToInt()`. A mismatch is diagnostic `R1001`.

## 6. Composite types

```robix
Array<Int>
List<String>
Span<Byte>
Pointer<Int>
Pointer<const Int>
Pointer<mut Byte>
&Int
&mut Int
[Int]
[Int; 4]
(Int, String)
Function(Int, Int) -> Int
String?
Option<Int>
Result
Result<Float, String>
Tuple
Enumerate<Int>
{ x: Int, y: Int }
```

`T?` and `Option<T>` are nullable. A non-nullable type is never `null`. Member access on a nullable value without a null check is `R1005`. The null-safe operator is `?.`.

`Result` and `Option` values expose `.tag` (`Int`) and `.value`. For `Result`, `tag` is `0` on `Error` and `1` on `Success`.

Constructors:

```robix
Success(value)
Error(value)
Some(value)
None()
Tuple(a, b)
```

Tuple fields read by the type checker are `.t0` and `.t1`.

Array literals and indexing:

```robix
Array<Int> values = [10, 20, 30];
values[2] = 12;
Int n = values.Length();
```

Indexing is bounds-checked unless the compiler proves the index is inside the array. A slice uses `..` or `..=`.

Generics:

```robix
public class Storage<T> {
    private T value;
    public Void Set(T newValue) {
        this.value = newValue;
    }
    public T Get() {
        return this.value;
    }
}

Storage<Int> box = new Storage<Int>();
```

A constraint list is `T: Iterable + Printable`. A `where` clause follows the parameter list. The native compiler monomorphizes a generic class (`Box$Int.Get`) when the instantiation is visible.

## 7. Operators

Highest precedence is 18. Lowest is 1.

| Precedence | Operators | Associativity |
| :---: | :--- | :--- |
| 18 | literals, `this`, `Base`, `(expr)`, `[a, b]`, `{ key: value }` | — |
| 17 | `.`, `?.`, call `()`, index `[]`, postfix `++` `--`, postfix `?` | left |
| 16 | `!` `-` `+` `~` `*` `&` `&mut` `++` `--` `await` `new` | right |
| 15 | `*` `/` `%` | left |
| 14 | `+` `-` | left |
| 13 | `<<` `>>` | left |
| 12 | `..` `..=` | none |
| 11 | `<` `<=` `>` `>=` `is` `as` | left |
| 10 | `==` `!=` | left |
| 9 | `&` | left |
| 8 | `^` | left |
| 7 | `\|` | left |
| 6 | `&&` | left, short-circuit |
| 5 | `\|\|` | left, short-circuit |
| 4 | `??` | left |
| 3 | `? :` | right |
| 2 | `=>` | right |
| 1 | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` | right |

Arguments, binary operands, array elements, and field initializers evaluate left to right.

String `+` concatenates. Two visible string literals become one global. Other concatenations call `rbx_strcat`. String `==` and `!=` use `strcmp`.

Pointer `+` and `-` are pointer arithmetic and require `Unsafe`.

Overloadable operators: `+` `-` `*` `/` `%` `==` `!=` `<` `<=` `>` `>=` `[]`.

```robix
public Int operator +(Point other) -> Point {
    return new Point();
}
```

## 8. Packages, imports, and visibility

```robix
package Robotics.Motors;

import Robix.Hardware.GPIO;
import Robotics.Robot as Bot;

public class Motor {
}
```

A package may be a qualified name followed by `;`, or a body:

```robix
package Robotics.Motors {
    Permissions {
        Robix.Hardware.GPIO;
    }
}
```

| Modifier | Same type | Derived type | Same module | Same package | Outside the build |
| :--- | :---: | :---: | :---: | :---: | :---: |
| `private` | yes | no | no | no | no |
| `protected` | yes | yes | no | no | no |
| `internal` | yes | yes in the build | yes | yes | no |
| `public` | yes | yes | yes | yes | yes |

Reading a private field from outside the type is `R2008`.

Top-level items, in any order after the package and imports: class, struct, enum, interface, trait, service, implement, function, `extern` function, constant, variable, type alias, macro, `Test`, `TestFile`. An attribute is `@Name` or `@Name(arguments)`.

An import of a bare name loads `<Name>.rbx` from the same directory.

## 9. Bindings

```robix
let name = "Roboticela";          // immutable, type inferred
var battery = 100;                // mutable, type inferred, stays Int
let speed: Float = 100;
const Int MAX_SPEED = 500;
Int motorSpeed = 100;
String robotName = "RobixBot";
```

`let` cannot be reassigned. `var` cannot change its static type. `const` is a compile-time constant and must be `UPPER_SNAKE_CASE` in strict lint. A variable must be definitely assigned before it is read.

A parameter may be `mut`, `ref`, or a variadic `...`:

```robix
public Void Update(mut Int speed, ref String name) {
}

Extern function PrintF(String format, ...) -> Int;
```

A parameter may have a default: `Int speed = 0`.

## 10. Functions

Two equivalent spellings:

```robix
public Function Add(Int a, Int b) -> Int {
    return a + b;
}

public Int Add(Int a, Int b) {
    return a + b;
}
```

A missing `->` type is inferred from `return` when the return is unambiguous. Overloads are distinguished by argument count.

Modifiers on a function: `public` and the other access modifiers, `static`, `abstract`, `override`, `consteval`, `async` / `Async`.

```robix
public consteval Int Square(Int n) -> Int {
    return n * n;
}

public async Function Download() -> String {
    String data = await Network.SendText("ping");
    return data;
}
```

A capability clause:

```robix
public Void OpenPin() -> Void requires Robix.Hardware.GPIO {
}
```

`Explain` sits between the signature and the body. `Description` is required. `Input` is required when there are parameters, and the names must match the signature. `Processing` is required. `Output` is required when the return type is not `Void`. `Errors` is required when the function returns `Result` or throws. `SideEffects` is required when the function mutates external state. A missing required field is warning `D3001`, and a build error when the project sets documentation required.

```robix
public Function Add(Int a, Int b) -> Int
Explain {
    Description: "Add two integers.";
    Input: a = "Left operand", b = "Right operand";
    Processing: "Return the sum.";
    Output: "The sum.";
} {
    return a + b;
}
```

`robix doc` writes that block to Markdown.

Entry point:

| Signature | Process result |
| :--- | :--- |
| `Main()` or `Main() -> Void` | exit code 0 |
| `Main() -> Int` | the returned integer is the exit code |
| `Main() -> String`, array, or object | the runtime prints the value, exit code 0 |
| `Main() -> Result` | `Success` exits 0; `Error` prints to stderr and exits 1 |

A freestanding image exports `Start` with the C calling convention unless `--entry` names another symbol. `extern` with a body is exported under its source name with the C calling convention. Other Robix functions use `fastcc`. `--no-main` emits the functions without a hosted `main`.

```robix
Extern function puts(String text) -> Int;
```

## 11. Classes, structs, interfaces, traits, enums

```robix
public abstract class Robot {
    public abstract Void Move();
}

public class WalkingRobot extends Robot {
    private String robotName;
    private Int batteryLevel;

    public WalkingRobot(String name) : Base() {
        this.robotName = name;
        this.batteryLevel = 100;
    }

    public override Void Move() {
        Print("Walking forward");
    }

    public ~WalkingRobot() {
    }
}

public class FlyingRobot : Robot {
    public override Void Move() {
        Print("Flying forward");
    }
}
```

`extends` and `:` are the same base-class clause. `implements` lists interfaces. A class may be `abstract`, `final`, or `partial`. Members may be nested classes, structs, and enums.

A constructor is the type name or the keyword `Constructor`. A destructor is `~TypeName` or `Destructor`. `this` is the instance. `Base(...)` chains to the base constructor.

```robix
public Int speed { get; set; }
```

`get` and `set` may have their own access modifier and a block, or end at `;`.

Class objects are a pointer. Word 0 is the vtable. Fields follow in alphabetical order, one word each. A `new` whose class is visible in the same function is a direct call. A call through a base type stays virtual. One interface's methods occupy the front of the vtable, sorted by name. A second interface uses its own slots. Locals whose type has a destructor are destroyed at the end of the scope, then freed. There is no garbage collector and no reference-counting runtime.

```robix
public struct Point {
    public Int x;
    public Int y;
}

public enum PinMode {
    Input,
    Output = 1,
    Alt(Int)
}

public interface Drivable {
    public Void Move();
}

public trait Iterable {
    public Function GetIterator() -> Int;
}
```

An enum variant is a name, an optional payload `(Type, ...)`, and an optional `= value`.

```robix
Service StatusService {
    public Int GetStatus(Int id) -> Int;
}

Implement StatusService {
    public Int GetStatus(Int id) -> Int {
        return id;
    }
}

Connect StatusService;
```

```robix
type Speed = Int;
type Box<T> = Storage<T>;
```

```robix
macro Twice(body) {
}
```

## 12. Statements and the ten loop forms

```robix
if (batteryLevel >= 80) {
    Print("Battery is full");
} else if (batteryLevel >= 30) {
    Print("Battery is sufficient");
} else {
    Print("Low battery");
}
```

Parentheses around the condition are optional: `if batteryLevel >= 80 { }`.

```robix
match result {
    Success(value) => Print(value),
    Error(message) => Print(message),
    _ => Print(0)
}
```

A match arm is `Pattern`, an optional `if` guard, `=>`, then a statement or an expression. Patterns: `_`, a literal, `Name(patterns)`, a tuple `(a, b)`, a record `{ a, b }`, or an identifier.

```robix
for (Int i = 0; i < 10; i = i + 1) {
    Print(i);
}

for i in 0..10 {
    Print(i);
}

for i in 1..=10 {
    Print(i);
}

for name in names {
    Print(name);
}

for (index, name) in names.Enumerate() {
    Print(index);
}

while (battery > 20) {
    battery = battery - 10;
}

do {
    ReadSensor();
} while (SensorIsActive());

repeat 5 {
    Print("Hello");
}

loop {
    ReadSensors();
    if (!isRunning) {
        break;
    }
    continue;
}
```

`0..10` is 0 through 9. `1..=10` is 1 through 10. `repeat N` runs N times, and runs zero times when N is less than or equal to 0. `for item in collection` on an array walks the elements. `break` and `continue` apply to the enclosing loop. `return` may carry a value.

```robix
try {
    throw 7;
} catch (Int code) {
    Print(code);
} finally {
    Print(0);
}

defer {
    Print(11);
}

using file = Open() {
}

Unsafe {
    Int speed = 100;
    Pointer<Int> speedPointer = &speed;
    *speedPointer = 250;
}

ASM("NOP");
ASM("movq $1, $0", out: y, in: x);
ASM("outb $0, $1", in: value, in: port, clobber: "memory");
```

`defer` runs its block when the scope exits, in reverse order of declaration, including on `return`. `using` runs its block for the bound name. `ASM` takes a string instruction, then optional `in:`, `out:`, and `clobber:` operands. The instruction is checked against the selected target. A mnemonic that the target does not have is `N9010`. A `$` immediate is escaped for LLVM when the statement has no operands.

`goto Label;` and `Label: statement` are in the grammar. The statement parser does not lower them.

## 13. Lambdas

```robix
Int total = doubled.Reduce(0, (sum, n) => sum + n);
Task first = Task.Run(() => Worker(1));
```

A single parameter may omit the parentheses: `n => n * 2`. A lambda written at the call site and invoked immediately is lowered. A lambda stored in a variable is not lowered by the native backend.

## 14. Memory, pointers, and unsafe

Safe code has no undefined behavior from an uninitialized read, an unchecked null dereference, or an out-of-range index that the compiler did not prove safe.

Raw pointers, pointer arithmetic, dereference, manual allocation, port I/O, CPU state, volatile device memory, atomics, and thread start require `Unsafe`. Using them outside `Unsafe` is `R1006`.

```robix
Unsafe {
    Pointer<Byte> memory = Memory.Allocate(1024);
    *memory = 42;
    Pointer<Byte> next = memory + 8;
    Memory.Free(memory);
}
```

| Call | Result | Where it runs |
| :--- | :--- | :--- |
| `Memory.Allocate(count)` | `Pointer<Byte>` | `malloc`. Requires `Unsafe` |
| `Memory.Free(pointer)` | `Void` | `free`. Requires `Unsafe` |
| `Memory.Copy(...)` | `Void` | Requires `Unsafe` |
| `Memory.Pages(...)` | `Pointer<Int>` | Freestanding page table. The interpreter stops |
| `Memory.Address(...)` | `Int` | Freestanding. The interpreter stops |
| `Memory.At(...)` | `Pointer<Byte>` | Freestanding. The interpreter stops |
| `Volatile.Load(pointer)` | `Int` | volatile 64-bit load. Requires `Unsafe` |
| `Volatile.Store(pointer, value)` | `Void` | volatile 64-bit store. Requires `Unsafe` |
| `Abort()` | does not return | `llvm.trap`, no allocation |
| `Panic()` | does not return | `llvm.trap`, no allocation |

`&` takes an address. `*` dereferences. Stack values, structs, and functions that do not escape stay off the heap. `new` of a class allocates the object. Automatic cleanup does not free memory that `Memory.Allocate` owns, and `Memory.Free` is not applied to a class object.

## 15. Built-in functions and methods

These names are known to the type checker without an import.

| Call | Signature | Behavior |
| :--- | :--- | :--- |
| `Print(value)` | `(Any) -> Void` | Prints `Bool`, `Int`, `Float`, and `String`. Integer print calls `printf` |
| `Input()` | `() -> String` | Reads one line |
| `Input<Int>()` | `() -> Int` | Reads one line and parses an integer. A line that is not an integer returns 0 |
| `Assert(condition)` | `(Bool) -> Void` | Assertion |
| `AssertTrue(condition)` | `(Bool) -> Void` | Assertion |
| `AssertFalse(condition)` | `(Bool) -> Void` | Assertion |
| `AssertEqual(expected, actual)` | `(Any, Any) -> Void` | Assertion |
| `AssertNear(expected, actual, tolerance)` | `(Any, Any, Any) -> Void` | Assertion |
| `Success(value)` | `(Any) -> Result` | Success tag |
| `Error(value)` | `(Any) -> Result` | Error tag |
| `Some(value)` | `(Any) -> Option` | Some tag |
| `None()` | `() -> Option` | None tag |
| `Tuple(a, b)` | `(Any, Any) -> Tuple` | Two-field tuple |
| `Abort()` | does not return | Trap |
| `Panic()` | does not return | Trap |

Methods accepted on a receiver:

| Method | Receiver | Result |
| :--- | :--- | :--- |
| `ToInt()` | number or byte | `Int` |
| `ToFloat()` | number | `Float` |
| `ToString()` | value | `String`. The native backend still reports this as an error |
| `Length()` | `String`, `Array`, slice | `Int`. String length is `strlen`. Array length is the length word |
| `Count()` | array or `Args` | `Int` |
| `Enumerate()` | array or list | `Enumerate<T>` |
| `tag` | `Result`, `Option` | `Int` |
| `value` | `Result`, `Option` | payload |
| `t0`, `t1` | `Tuple` | `Int` |

`Assert.Equal`, `Assert.True`, `Assert.False`, and the other `Assert.*` calls lower as assertions.

## 16. Standard library packages (`.rbx`)

These are the packages under `StdLib/Robix/`. Each one has a matching `.test.rbx`. `robix test StdLib/Robix` runs them on the interpreter and on the native compiler.

### `Robix.Core`

| Function | Signature |
| :--- | :--- |
| `Abs` | `(Int x) -> Int` |
| `Min` | `(Int a, Int b) -> Int` |
| `Max` | `(Int a, Int b) -> Int` |
| `Clamp` | `(Int value, Int low, Int high) -> Int` |

### `Robix.Math`

| Function | Signature |
| :--- | :--- |
| `Gcd` | `(Int a, Int b) -> Int` |
| `IntegerSqrt` | `(Int n) -> Int` |

`Math.Abs`, `Math.Min`, `Math.Max`, and `Math.Clamp` are also integer intrinsics. They do not require the import when written as `Math.Abs(x)`.

### `Robix.String`

| Function | Signature |
| :--- | :--- |
| `Digit` | `(Int n) -> String` |
| `FormatInt` | `(Int n) -> String` |

### `Robix.Collections`

| Function | Signature |
| :--- | :--- |
| `SumStored` | `() -> Int` |

### `Robix.IO`

| Function | Signature |
| :--- | :--- |
| `SaveText` | `(String path, String text) -> Int` |
| `LoadText` | `(String path) -> String` |

### `Robix.Memory`

| Function | Signature |
| :--- | :--- |
| `SeparatedBytes` | `() -> Int` |

Allocates 16 bytes, writes two bytes eight bytes apart, reads them back, frees the buffer, and returns the sum.

### `Robix.Threading`

| Function | Signature |
| :--- | :--- |
| `Pair` | `(Int left, Int right) -> Int` |

Native execution starts two operating-system threads and interlocked-adds both values. The interpreter applies both additions on the calling thread and returns the same sum.

### `Robix.Atomics`

| Function | Signature |
| :--- | :--- |
| `Bump` | `(Int start, Int delta) -> Int` |

### `Robix.Hardware`

| Function | Signature |
| :--- | :--- |
| `ReadPin` | `(Int pin) -> Result` |

### `Robix.Robotics`

| Function | Signature |
| :--- | :--- |
| `PidStep` | `(Int kp, Int ki, Int kd, Int error, Int integral, Int previous) -> Int` |
| `RotateX` | `(Int quarter, Int x, Int y) -> Int` |
| `RotateY` | `(Int quarter, Int x, Int y) -> Int` |
| `QuatX` | `(Int w1, Int x1, Int y1, Int z1, Int w2, Int x2, Int y2, Int z2) -> Int` |
| `QuatW` | `(Int w1, Int x1, Int y1, Int z1, Int w2, Int x2, Int y2, Int z2) -> Int` |

`PidStep` is `kp * error + ki * integral + kd * (error - previous)`. `RotateX` and `RotateY` rotate a quarter turn 0, 1, 2, or 3. `QuatW` and `QuatX` are the w and x components of a quaternion product.

### `Robix.Encoding`

| Function | Signature |
| :--- | :--- |
| `EncodeNumber` | `(Int value) -> String` |
| `Pack` | `(Int high, Int low) -> Int` |
| `HighByte` | `(Int packed) -> Int` |
| `LowByte` | `(Int packed) -> Int` |

`EncodeNumber` returns `{"n":<value>}`. `Pack` is `(high << 8) | (low & 255)`.

### `Robix.Env`

| Function | Signature |
| :--- | :--- |
| `Get` | `(String name) -> String` |
| `Exists` | `(String name) -> Int` |
| `Set` | `(String name, String value) -> Int` |
| `Add` | `(String name, String value) -> Int` |
| `Remove` | `(String name) -> Int` |

`Get` of a missing name prints `warning: env <name> not found`. An empty value prints `warning: env <name> is empty`. `Exists` returns `0` or `1` and does not warn. `Add` writes the name into `.env.common`.

## 17. Intrinsics

These are called as `Receiver.Member(...)`. They do not need a package import. Hardware and CPU calls require `Unsafe` except where noted.

### `File`

| Call | Result |
| :--- | :--- |
| `File.WriteText(path, text)` | `Bool` |
| `File.ReadText(path)` | `String` |

`robix run` stores files in a map. `robixc` reads and writes the host disk.

### `Args`

| Call | Result |
| :--- | :--- |
| `Args.Count()` | `Int` |
| `Args.Get(index)` | `String` |

`robix run Source/Program.rbx alpha beta` makes `Args.Count()` return `2`.

### `Env`

Same five calls as `Robix.Env`: `Get`, `Exists`, `Set`, `Add`, `Remove`.

### `Network`

| Call | Result |
| :--- | :--- |
| `Network.SendText(text)` | `String` |
| `Network.Call(...)` | call on the linked network runtime |

A project with `Network: true` links `NetworkRuntime.obj`. A project without it omits that runtime. The Stage K contract shape is `GetStatus(Int) -> Int` and `BatteryLow(Int) -> Void`.

### `Math`

| Call | Result |
| :--- | :--- |
| `Math.Abs(x)` | `Int` |
| `Math.Min(a, b)` | `Int` |
| `Math.Max(a, b)` | `Int` |
| `Math.Clamp(value, low, high)` | `Int` |

### `Thread` and `Atomic`

| Call | Result |
| :--- | :--- |
| `Thread.AddPair(slot, left, right)` | `Int`. Requires `Unsafe` |
| `Atomic.FetchAdd(slot, delta)` | `Int`, the previous value. Requires `Unsafe`. Native code is `atomicrmw add ... seq_cst` |

`Thread.Sleep` and `Time.NowMicroseconds` are recognized by the LLVM intrinsic emitter.

### `GPIO`, `PWM`, `UART`, `SPI`, `I2C`, `CAN`, `USB`, `PCIe`, `DMA`, `Sensor`, `Motor`, `Interrupt`, `HardwareTimer`

| Call | On Windows |
| :--- | :--- |
| `GPIO.Read(pin)` | returns `1` and writes that the device is not available. Requires `Unsafe` |
| `PWM.SetDuty` | returns `1`, device not available |
| `UART.Write` | returns `1`, device not available |
| `SPI.Transfer` | returns `1`, device not available |
| `I2C.Write` | returns `1`, device not available |
| `CAN.Send` | returns `1`, device not available |
| `USB.Write` | returns `1`, device not available |
| `PCIe.Read` | returns `1`, device not available |
| `DMA.Start` | returns `1`, device not available |
| `Sensor.Read` | returns `1`, device not available |
| `Motor.SetSpeed` | returns `1`, device not available |
| `Interrupt.Enable` | returns `1`, device not available |
| `HardwareTimer.Read` | returns `1`, device not available |

These calls do not read a pin, transfer a byte, or move a motor on Windows. `GPIO.Write` is recognized by the LLVM intrinsic emitter. The interpreter reports the same not-available result for `GPIO.Read`.

### Freestanding CPU, ports, timer, console

The interpreter stops on these and tells you they run in the freestanding image. They require `Unsafe`.

| Call | Result |
| :--- | :--- |
| `Port.Write8(port, value)` | `Void` |
| `Port.Read8(port)` | `Int` |
| `Port.Read16(port)` | `Int` |
| `Console.Write(byte)` | `Void`. Board serial port |
| `Cpu.Halt()` | `Void`. `hlt` on x86-64, `wfi` on AArch64, ARM, and RISC-V |
| `Cpu.Cli()` | `Void` |
| `Cpu.Sti()` | `Void` |
| `Cpu.ReadCR3()` | `Int` |
| `Cpu.WriteCR3(value)` | `Void` |
| `Cpu.ReadRSP()` | `Int` |
| `Cpu.ExchangeRSP(value)` | `Int` |
| `Cpu.SavedRSP()` | `Int` |
| `Cpu.Lgdt(...)` | `Void` |
| `Cpu.Lidt(...)` | `Void` |
| `Cpu.BootInfo()` | `Int` |
| `Cpu.Function(...)` | `Int` |
| `Cpu.Handler(...)` | `Int` |
| `Cpu.Run(...)` | `Void` |
| `Timer.Arm(handler)` | `Void`. Installs the gate and programs the PIC |
| `Timer.Preempt(...)` | `Void` |
| `System.Call(byte)` | `Void`. `int 0x80` in the kernel image |

## 18. Host library modules

These C++ modules live under `StdLib/` and implement library behavior for the runtime. They are not separate `.rbx` packages. Names below are the public types and operations in those headers.

### Core (`StdLib/Core`)

`ConsoleIO`: `Print` for text, integer, float, and bool; `Input`; `TryInputInt`; `TryInputFloat`; `TryInputBool`; `TryInputString`.

`NumericOps`: `Abs`, `ToFloat`, `ToInt`, `ToString`, `Round`, `Floor`, `Ceiling`, `Sqrt`, `Pow`, `IsNaN`, `IsInfinite`.

`StringOps`: `Length`, `IsEmpty`, `ToUpper`, `ToLower`, `Trim`, `Contains`, `StartsWith`, `EndsWith`, `Replace`, `Split`, `Substring`, `IndexOf`, `ToInt`, `ToFloat`.

`BooleanOps`: `Not`, `And`, `Or`.

`Vec3`: `x`, `y`, `z`, `+`, `-`, `*`, `Dot`, `Cross`, `Magnitude`, `Normalized`.

`Quaternion`: `w`, `x`, `y`, `z`, `FromAxisAngle`, `*`, `Normalized`, `Rotate`.

`MathModule`: `PI`, `Min`, `Max`, `Clamp`, `ReLU`, `Sin`, `Cos`, `Tan`. Angles are radians.

`LinearRegex`: construct from a pattern string and match text.

`RobixDateTime`: `Now`.

`MonotonicClock`: `NowNanoseconds`, `NowMicroseconds`, `SleepMilliseconds`.

### Collections (`StdLib/Collections`)

`List<T>`: `Add`, `Remove`, `Get`, `Set`, `Contains`, `Find`, `Filter`, `Map`, `Reduce`, `Sort`, `Reverse`, `Clear`, `Count`, `IsEmpty`, `Enumerate`, `GetIterator`.

`Dictionary<K, V>` and `HashMap<K, V>`: `Set`, `Get`, `Contains`, `Remove`, `Clear`, `Count`, `IsEmpty`.

`HashSet<T>`: `Add`, `Remove`, `Contains`, `Clear`, `Count`, `IsEmpty`.

`Stack<T>`: `Push`, `Pop`, `Peek`, `Count`, `IsEmpty`.

`Queue<T>`: `Enqueue`, `Dequeue`, `Count`, `IsEmpty`.

`PriorityQueue<T>`: `Enqueue`, `Dequeue`, `Count`, `IsEmpty`. Minimum priority.

`Graph<T>`: `AddEdge`, `HasPathBFS`.

`Matrix<T>`: `Rows`, `Cols`, `At`, `Multiply`, `Transpose`.

`Tensor<T>`: `Shape`, `ElementCount`, `Data`.

`Iterator<T>` / `Iterable<T>`: `Next() -> Option<T>`, `GetIterator() -> Iterator<T>`.

### JSON and files (`StdLib/Data`)

`JsonValue`: `MakeNull`, `MakeBool`, `MakeInt`, `MakeFloat`, `MakeString`, `MakeArray`, `MakeObject`. Kinds are null, bool, int, float, string, array, object.

JSON parse and stringify, JSONL lines, required-field checks, CSV rows, a binary frame of id plus payload, and run-length compression.

`MemoryTable`: `CreateTable`, `InsertRow`, query, `Count`.

`TextFile`: `WriteAllText`, read-all, exists, `RemoveFile`.

### Security and network (`StdLib/Security`)

`CapabilitySandbox`: `GrantCapability`, `RevokeCapability`, `EnforceCapability`.

`SecretBuffer`: construct from secret bytes, `ZeroizeNow`.

HMAC-SHA256 sign and verify. AES-style encrypt and decrypt with a nonce.

`SecurityAuditResult.AuditDependencies` checks package name, version, SHA-256, and signature, and records an SBOM.

`HttpServer`: `RegisterRoute`. Request fields are method, path, body, headers.

`TopicBus`: `Subscribe`, `Publish`.

### AI and scientific (`StdLib/AI`)

`SIMDVec4f`: four floats, horizontal sum.

`AutodiffTape`: `CreateVariable`, `Add`, `Multiply`, `ReLU`, `Backward`.

`ActivationFunctions.Apply`, including ReLU and Softmax.

`DenseLayer`: input dimension, output dimension, weights, biases, activation.

`SequentialNeuralModel`: `AddLayer`, forward inference.

`GrayscaleImage2D`: `SetPixel`, get pixel, 3×3 convolution.

`AudioSignalProcessor`: complex spectrum and RMS.

`ComplexSample`: `real`, `imag`.

### Testing (`StdLib/Testing`)

`Assert.Equal`, `Assert.NotEqual`, `Assert.True`, `Assert.False`, `Assert.Null`, `Assert.NotNull`, `Assert.Throws`, `Assert.ThrowsAsync`, `Assert.Success`, `Assert.Error`, `Assert.Contains`, `Assert.Timeout`.

Per-file tests are `Name.rbx` and `Name.test.rbx`. `RequireTests: true` in the project makes `robix test` and `robix build` exit 1 when a source file has no test file. A lone `.rbx` with no project still builds.

## 19. Tests

```robix
Test "Addition returns correct result" {
    Assert.Equal(Add(10, 20), 30);
}

TestFile "Motor.test.rbx" {
    Covers = "Motor.rbx";
    Test "Battery cannot exceed maximum" {
        Assert.Equal(battery.Level, 100);
    }
}
```

`robix test` runs the interpreter and the native compiler and compares them. Bytecode is versioned. A bytecode module whose version is not the current one is refused before execution. Tests that call freestanding port I/O are listed in `Tests/Conformance/NATIVE_ONLY.md` and stop on the interpreter.

## 20. Concurrency

The language defines threads, tasks, async functions, `await`, atomics, mutexes, channels, and cancellation. The calls the compiler lowers today are `Thread.AddPair`, `Atomic.FetchAdd`, `async` / `await` as syntax, and `defer` cleanup on scope exit.

A mutable location shared by threads is an `Atomic` fetch-add or is otherwise inside `Unsafe`. `Thread.AddPair` on the native compiler starts two Windows threads. The interpreter performs both additions on the caller and returns the same sum.

Structured cancellation, `Mutex`, `ReadWriteLock`, `Semaphore`, `Channel<T>`, `Task.Run`, `Task.Wait`, and `Process.Start` are specified. They are not separate lowered intrinsics in the native backend beyond the calls in this file.

## 21. RobixUI

Documents are XML, extension `.rbxu`. The root is `RobixUI`.

```xml
<?xml version="1.0"?>
<RobixUI Version="1.0">
  <Window Name="RobotDashboard" Title="Roboticela Robot Control" Width="1000" Height="700">
    <Column Spacing="16">
      <Text Value="Robot Dashboard" FontSize="28" FontWeight="Bold"/>
      <Row Spacing="12">
        <Button Name="StartButton" Text="Start Robot" OnClick="StartRobot"/>
        <Button Name="StopButton" Text="Stop Robot" OnClick="StopRobot"/>
      </Row>
      <Text Name="RobotStatus" Value="Disconnected"/>
    </Column>
  </Window>
</RobixUI>
```

`robix ui validate File.rbxu --code File.rbx` checks that each `OnClick` names a Robix function and that the function type matches. A missing function or a wrong signature exits 1.

Grammar components: `Window`, `Page`, `View`, `Panel`, `Container`, `Column`, `Row`, `Grid`, `Stack`, `Button`, `Text`, `TextField`, `Image`, `Icon`, `List`, `Table`, `Menu`, `Dialog`, `Tabs`, `Slider`, `Switch`, `Checkbox`, `RadioButton`, `ProgressBar`, `Chart`, `Canvas`, `Template`, `Slot`, `Style`. A custom component name is PascalCase. A screen can be split into components, import them, pass values in, and repeat or choose elements while the screen is built.

Grammar attributes: `Version`, `Name`, `Title`, `Value`, `Text`, `Width`, `Height`, `MinWidth`, `MinHeight`, `MaxWidth`, `MaxHeight`, `Spacing`, `Padding`, `Margin`, `Alignment`, `FontSize`, `FontWeight`, `FontFamily`, `Color`, `Background`, `Border`, `BorderRadius`, `Shadow`, `Theme`, `Visible`, `Enabled`, `If`, `ForEach`.

Events: `OnClick`, `OnDoubleClick`, `OnMouseEnter`, `OnMouseLeave`, `OnKeyDown`, `OnKeyUp`, `OnFocus`, `OnBlur`, `OnValueChanged`, `OnLoaded`, `OnClosed`.

Bindings: `Bind:Property` is one-way. `BindTwoWay:Property` is two-way. A value may be `{Property}`, `{=Property}`, or `{expression}`.

Layout: horizontal (`Row`), vertical (`Column`), grid, stack, absolute, spacing, padding, margin, alignment, min and max size.

Screen sizes follow the browser width. The default edges are `sm` 640, `mx` 960, `xl` 1280, and `2x` 1536. A named size applies at that width and wider. `sm-mx`, `mx-xl`, and `xl-2x` apply only in the band between those edges. `900` and `at-900` are a custom width and wider. `900-1200` is only that custom band. `Screen Sizes="sm:480 mx:800 xl:1200 2x:1600"` replaces the defaults.

A property with a size suffix changes only on those screens: `Width.sm="100%"`, `FontSize.mx="22"`, `Width.sm-mx="320"`, `Padding.at-900="24"`, `Width.900-1200="480"`. A dot joins several sizes that share one value, so `FontSize.sm.xl="28"` sets that size on `sm` and on `xl`. The same value can also be written once for several sizes:

```xml
<At Size="sm, xl" FontSize="28" Color="#FFFFFF"/>
<At Size="900-1200" Width="480"/>
```

`Only="sm"` draws the component only on that size. `Only="sm.xl"`, `Only="mx-xl"`, and `Only="900-1200"` do the same for a list or a band. The base value, with no suffix, is what the other widths use.

`Visibility="false"` hides a component, and `Visibility="true"` shows it. `Visible` accepts the same two values. A size suffix hides or shows it on that screen only: `Visibility.sm="false"` hides it from `sm` upward, and `Visibility.xl="true"` shows it again from `xl` upward. `0`, `no`, `off`, and `hidden` hide. `1`, `yes`, `on`, and `visible` show.

Components, data, and small logic are expanded when the screen is built. `Import Source="Card.rbxu"` reads another document. `Component Name="Card"` defines a piece that is not drawn by itself. Using `<Card Title="Robix"/>` pastes that piece in, and `{Title}` becomes the value that was passed. Children of the use site fill `<Slot/>`.

```xml
<Component Name="Card">
  <Column Spacing="8">
    <Text Value="{Title}" FontSize="20"/>
    <Text Value="{Detail}"/>
    <Slot/>
  </Column>
</Component>

<Data Name="robots">
  <Item Name="Walker" Speed="10"/>
  <Item Name="Flyer" Speed="30"/>
</Data>

<For Each="robot" In="{robots}" Index="number">
  <Card Title="{robot.Name}" Detail="{robot.Speed}"/>
  <If Test="{robot.Speed > 20}">
    <Text Value="Fast"/>
    <Else>
      <Text Value="Slow"/>
    </Else>
  </If>
</For>
<For Each="step" From="1" To="3">
  <Text Value="{step}"/>
</For>
```

`{...}` can use a name, `record.Field`, text in quotes, numbers, `true`, `false`, `+`, `-`, `*`, `/`, `==`, `!=`, `>`, `<`, `>=`, `<=`, `&&`, `||`, and `!`. `For` also accepts a comma list, `In="Ali, Ahmed, Sara"`. `Let` names a value for its children. `Data` with `Value` stores one value. A `For` stops at 1000 items. A component name cannot be a built-in element name. These steps run while the document is expanded, so the drawn screen contains the repeated elements.

Motion attributes work on every visual element: `Window`, `Screen`, `Page`, `Board`, `Column`, `Row`, `Container`, `Text`, `Heading`, `Subhead`, `Body`, `Paragraph`, `Caption`, `Eyebrow`, `Label`, `Button`, `Anchor`, `IconButton`, `Switch`, `SwitchGroup`, `SwitchItem`, `SegmentedControl`, `SegmentedToolbar`, `Icon`, `Svg`, and `Image`.

```xml
<Button Text="Go" Animate="fade-up" WhileHover="lift" WhileTap="press" Transition="duration:400 ease:spring"/>
<Text Value="Hello" Initial="opacity:0 y:16" Animate="opacity:1 y:0" Exit="opacity:0"/>
```

Entrance presets: `fade`, `fade-up`, `fade-down`, `fade-left`, `fade-right`, `slide-up`, `slide-down`, `slide-left`, `slide-right`, `zoom-in`, `zoom-out`, `pop`, `grow`, `shrink`, `flip-x`, `flip-y`, `rotate-in`, `spin`, `pulse`, `bounce`, `shake`, `swing`, `float`, `wobble`, `blur-in`, `rise`, `drop`, `expand`, `heartbeat`, `flash`, `jello`, `rubber`, `tada`, `ping`.

`WhileHover`, `WhileTap`, and `WhileFocus` also accept `lift`, `press`, `grow`, `shrink`, `tilt`, `glow`, `pulse`, `spin`, `shake`, and `none`. A property list uses `opacity`, `x`, `y`, `z`, `scale`, `scaleX`, `scaleY`, `rotate`, `rotateX`, `rotateY`, `skewX`, `skewY`, and `blur`.

`Transition` takes `duration`, `delay`, `ease`, `repeat`, and `direction`. Ease values are `linear`, `ease`, `ease-in`, `ease-out`, `ease-in-out`, `spring`, `bounce`, `anticipate`, `smooth`, `snappy`, and `gentle`. `spring` is a spring-shaped curve. `WhileInView` plays when the element enters the viewport. `Drag="true"` follows the pointer and glides on release. `Drag="x"` and `Drag="y"` lock one axis. `Robo.leave(element)` plays `Exit` and then hides the element. `spin`, `pulse`, `bounce`, `float`, `heartbeat`, `flash`, and `ping` repeat until `Repeat` says otherwise. The Windows window paints the still layout. These animations play on the web page.

Themes: `light` and `dark`. `Foundation.rbxu` supplies colors, space, and type. The default face is Inter (`Inter-Regular`, `Inter-Medium`, `Inter-SemiBold`, `Inter-Bold`, `Inter-Italic`). `system`, `serif`, and `mono` select Segoe UI, Georgia, and Consolas when the foundation has no font of that name.

Component schemas under `RobixUI/Compiler/Components` add these elements and attributes on top of the grammar:

| Component | Attributes |
| :--- | :--- |
| `Screen` | `id`, `path` (`route`), `name` (`title`), `theme` (`dark` or `light`), `transparent`, `place` (`center`, `top`, `bottom`, `left`, `right`, corners, `absolute`), `x`, `y`, `width`, `height`, `minWidth`, `minHeight`, `maxWidth`, `maxHeight`, `minSize`, `maxSize`, `radius`, `foundation`, `contentWidth`, `gutter`, `border`, `overflow`, `overflowX`, `overflowY`, `clip`, `background`, `borderColor`, `display` (`flex`, `grid`, `stack`), `direction` (`row`, `column`, and the reverse forms), `justify`, `movable`, `resizable`. A screen with no size is 640 by 400, theme dark |
| `Board` | same document shape as `Screen` when used as the board surface |
| `Container` | layout container |
| `Button` | `id`, `variant` (`primary`, `secondary`, `action`, `ghost`, `danger`), `size` (`sm`, `md`, `lg`), `fullWidth`, `loading`, `loadingText`, `leading`, `trailing`, `preview` (`hover`, `focus`, `active`), `disabled`, `type` (`button`, `submit`, `reset`), `content`, `path`. Children: `Icon`, `Svg` |
| `Anchor` | text link with the same destination, icons, disabled state, and preview state as `Button`. `variant` is `inline`, `subtle`, `underline`, `plain`, `standalone`, or `arrow`. `size` is `inherit`, `sm`, `md`, or `lg`. `tone` is `brand` or `danger`. `arrow` is `end` or `start`. The Windows window paints the words. The circle, sliding chevron, and launch mark are on the web page |
| `IconButton` | icon button, 44 by 44 in the controls demo |
| `Switch` | checked state, `aria-checked` on the web page |
| `SegmentedControl` | selected segment |
| `Text`, `Heading`, `Subhead`, `Body`, `Paragraph`, `Caption`, `Eyebrow`, `Label` | shared text renderer. A `Span` carries a color |
| `Image` | web picture. `source` (`src`), `alt`, `load` (`content` with the page, `ready` after the page loads, `lazy` near the viewport), `resize` (`false` keeps one file). Web compilation writes smaller widths and a `srcset`. The interpreter copies the original file and does not resize. The Windows window does not paint it |
| `Icon`, `Svg` | icon and SVG children |
| `Scrollbar` | scrollbar schema |

Also painted on Windows and on the web: `ConfirmAction`, `ThemeToggle`, `ThemeIconToggle`.

Renderers:

| Command | What it does |
| :--- | :--- |
| `robix ui render File.rbxu -o out.bmp` | software framebuffer BMP |
| `robix ui window File.rbxu` | Windows window, then closes after paint |
| `robix ui window File.rbxu --dev` | Windows window that stays open and reloads |
| `robix ui web File.rbxu -o out.html` | HTML page |
| `robix ui --dev` | development host used by the UI demos |

The Windows host handles DPI, mouse move, leave, down, up, double-click, Tab focus, Return, Space, mouse wheel, size, paint, and close. The web page listens for pointer, double-click, wheel, key, resize, and hash changes, and posts them to `/robix-event`. The framebuffer fills rectangles and discs. It does not draw rounded corners, icon-pack glyphs, a spinner animation, a focus halo, or letter-spacing. The web page applies those styles. Linux, macOS, Android, and iOS platform folders have no renderer.

A Robix controller binds by name:

```robix
package Application;

public class MainController {
    public Void StartRobot() {
        Print("Robot started");
    }
    public Void StopRobot() {
        Print("Robot stopped");
    }
}
```

## 21.1 Robix3D

Documents are XML, extension `.r3d`. The root is `Robix3D`. A RobixUI document cannot contain Robix3D elements, and a Robix3D document cannot contain RobixUI elements. The two meet only through a host: `<View3D Source="Scene.r3d"/>` inside RobixUI, and `<UI Source="Panel.rbxu"/>` inside Robix3D.

Units default to meters. Axes are right-handed and Y is up. Rotation is radians unless `Degrees="true"`. The web page is Three.js 0.170.0. The Windows window paints the hosted scene as a labeled panel.

A scene can contain a camera, lights, groups, meshes, primitives, materials, textures, glTF models, environment lighting, a timeline, sequences, constraints, physics, shaders, modifiers, and procedural components. `Import` reads another `.r3d` file. `Component`, `Instance`, `Extends`, and `Override` reuse objects. `Generator` and `ForEach` repeat them, and `{Name}` is a numeric expression. Geometry with a negative size is rejected.

## 22. Project file

A `.rbxb` file:

```text
Project {
    Name = "HelloApp";
    Output = "Executable";
    Entry = "Source/Program.rbx";
    Network = true;
    RequireTests = true;
    Sources {
        "Source/Main.rbx";
    }
}
```

`Output` values used by the toolchain: `Executable`, `Kernel`, `Firmware`, library, static library, shared library, object, package. `robixc File.rbxb --explain` prints the compile order and does not write a binary. `--dry-run` prints `dry-run: compile was not started`.

`robix build` is incremental. An unchanged second build prints `incremental: unchanged`. Editing a file that `Main` imports rebuilds the executable.

`robix new` asks for a category, then a kind. **Application** is a program a person runs: `Console` (a terminal), `Window` (a `.rbxu` file and a small test), or `Network` (clients and servers). A console program compiles for `windows`, `linux`, `macos`, `android`, and `ios`, and is refused for `web`. A window or a network program can also compile for `web` when the project has a `.rbxu` screen. Web compilation has two forms. `--web script` is the default: the screen stays HTML and CSS, and the Robix program is converted to JavaScript with the same behavior as the interpreter. `--web assembly` keeps that HTML and CSS and compiles the program to WebAssembly (`app.wasm`). `robix run --target web` always uses the script form. A feature that cannot run in a browser, such as inline assembly, CPU and port access, or a hardware bus, prints `warning W7001` and is skipped. The rest of the program still compiles and runs. This machine interprets `windows` and `web`, and compiles `windows`. The other platforms are refused here with a direct error. **Canvas** is classified from the file. `Network` or `SendText` is a network program and can use a `.rbxu` in the same folder. A matching `.rbxu` (the same name, the `Source`/`UI` pair, or the only `.rbx` in the folder) is a window. `Print`, `Input`, or `Args` is a console program. The tool prints a warning naming that choice, then applies the same web rule. **Canvas** is an empty folder. `robix run File.rbx` runs one file, `robix run A.rbx B.rbx` runs each file, and `--image` builds a machine image. **Bedrock** is the machine itself: `Kernel`, `OS`, `BIOS`, `Driver`, `Bootloader`, or `Firmware`. It builds an image and is refused when the target is an operating system. If a file uses machine code (`Cpu`, `Port`, `Console.Write`) and the target is a platform, or platform code (`Print`, `File`, `Args`) and the target is an image, `robix` and `robixc` both stop with a direct error. `CLI` still means Console, `GUI` still means Window, and `Blank` still means Canvas. A Window project and a Network project each get an `Assets` folder at the project root. `robix new file Sensor class` writes the source and the test. A Network scaffold writes client and server projects, `Common/Source` interfaces, a `.rbxu` for a GUI service, `.env.common`, and that `Assets` folder.

## 23. Diagnostics

| Code | Meaning |
| :--- | :--- |
| `R1001` | Type mismatch. The message names `Expected` and `Received` |
| `R1004` | Type mismatch with a conversion suggestion |
| `R1005` | Member access on a nullable value |
| `R1006` | Unsafe operation outside `Unsafe` |
| `R2005` | Integer overflow in a debug or safety profile |
| `R2008` | Access to a private member |
| `D3001` | Required `Explain` field is missing |
| `L0004` | Constant name is not `UPPER_SNAKE_CASE` |
| `P1003` and other `P` codes | Parse errors, including `Explain`, ASM operands, and macro shape |
| `N9001` | An OS intrinsic the native backend does not lower |
| `N9010` | Assembly mnemonic is not valid for the target |
| `W7001` | A feature cannot run on the web target. It is skipped, and compilation continues |

Every diagnostic carries a file, line, and column.

## 24. Execution

Development default is the interpreter: `robix run File.rbx`. Production is the native compiler: `robix build --release` or `robixc -O3`. `robix run File.rbx --engine native` compiles and runs. `--engine auto` selects. A native compilation failure is not replaced by the interpreter.

Both engines share the lexer, parser, AST, name resolution, and type checker. The interpreter executes bytecode. The compiler lowers the same program through SSA IR to LLVM, then `opt -O3` and `clang -O3`. Bytecode is validated before execution.

`robix repl` reads lines, writes them to a temporary `.rbx`, and checks them with the same parser and type checker. `Print(40 + 2);` then `run` prints `42`.

`robix debug` records source lines. `robix debug --attach` uses the Windows debug API and does not open a debugger window. A normal `robix build` does not emit those line calls. `robix lsp` answers hover from the `Explain` block. The editor extension is `roboticela.robix`.
