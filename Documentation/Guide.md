# Robix guide

This guide matches the compiler that prints `robix version 1.0.0`. The programs below were compiled with `robixc -O3` and run with `robix run`. `match`, `try`, lambdas, loading a package from another file, a class with two interfaces, and calling a Robix function from C are not compiled. `IntList` in `StdLib/Hosted.rbx` holds four integers. `File` in `robix run` is an in-memory map; `robixc` reads and writes the host disk. A line that is not an integer makes `Input<Int>()` return 0.

The recommended declaration is `Function Name(Int x) -> Int`. `Int Name(Int x)` is the same function.

## Hello

`Examples/GettingStarted/Hello.rbx`

```robix
package Examples.GettingStarted;

public Int Add(Int a, Int b) {
    return a + b;
}

public Function Main() -> Void {
    Print("Hello, Robix");
    Print(Add(20, 22));
}
```

```text
robixc -O3 Examples/GettingStarted/Hello.rbx -o Build/Hello.exe
robix run Examples/GettingStarted/Hello.rbx
```

Both print:

```text
Hello, Robix
42
```

## Struct

`Examples/GettingStarted/Point.rbx`

```robix
package Examples.GettingStarted;

public struct Point {
    public Int x;
    public Int y;
}

public Function Main() -> Void {
    Point p = new Point();
    p.x = 3;
    p.y = 4;
    Print(p.x + p.y);
}
```

```text
robixc -O3 Examples/GettingStarted/Point.rbx -o Build/Point.exe
robix run Examples/GettingStarted/Point.rbx
```

Both print `7`.

## Loop

`Examples/GettingStarted/Loop.rbx`

```robix
package Examples.GettingStarted;

public Function Main() -> Void {
    Int sum = 0;
    for (Int i = 1; i <= 4; i = i + 1) {
        sum = sum + i;
    }
    Print(sum);
}
```

```text
robixc -O3 Examples/GettingStarted/Loop.rbx -o Build/Loop.exe
robix run Examples/GettingStarted/Loop.rbx
```

Both print `10`.

## Errors

A failed division returns `Result`. `tag` is `0` for `Error` and `1` for `Success`. `value` is the payload. `Examples/GettingStarted/Divide.rbx`:

```robix
package Examples.GettingStarted;

public Function Divide(Int a, Int b) -> Result {
    if (b == 0) {
        return Error(0);
    }
    return Success(a / b);
}

public Function Main() -> Void {
    Result bad = Divide(1, 0);
    Result ok = Divide(20, 4);
    Print(bad.tag);
    Print(ok.value);
}
```

```text
robixc -O3 Examples/GettingStarted/Divide.rbx -o Build/Divide.exe
robix run Examples/GettingStarted/Divide.rbx
```

Both print `0`, then `5`.

A type mismatch names both types. `Examples/GettingStarted/TypeMismatch.rbx` assigns a string to an `Int`. `robixc -O3 Examples/GettingStarted/TypeMismatch.rbx -o Build/TypeMismatch.exe` stops with `R1001`, `Expected: Int`, and `Received: String`.

## Input

`Examples/GettingStarted/Ask.rbx` reads one line, then one integer.

```robix
package Examples.GettingStarted;

public Function Main() -> Void {
    String name = Input();
    Print(name);
    Int n = Input<Int>();
    Print(n + 1);
}
```

```text
robix run Examples/GettingStarted/Ask.rbx
robixc -O3 Examples/GettingStarted/Ask.rbx -o Build/Ask.exe
```

The lines `Ada` and `41` print `Ada` and `42`.

## Array, string, math, list, and file

`StdLib/Hosted.rbx` is a program, not an empty namespace. It defines `Abs`, `Min`, `Max`, `Clamp`, and `IntList`, then calls `Array.Length`, `String.Length`, `Math.Abs`, `Math.Clamp`, `Math.Max`, and `File.WriteText` / `File.ReadText`.

```text
robixc -O3 StdLib/Hosted.rbx -o Build/Hosted.exe
robix run StdLib/Hosted.rbx
```

Both print `45`, `hosted`, `4`, `10`, and `9`. There is no `import` of this file into another program yet. `Math.Abs`, `Math.Min`, `Math.Max`, `Math.Clamp`, `Length` on a string or an array, and `File.ReadText` / `File.WriteText` can be called from any file without that copy.

## A new program

```text
robix new HelloApp
robix run HelloApp/Main.rbx
robixc -O3 HelloApp/Main.rbx -o Build/HelloApp.exe

A copy of that first project is kept at `Examples/GettingStarted/HelloApp`.
```

`robix new` writes one `Main.rbx`. It prints `Hello from Robix`.
