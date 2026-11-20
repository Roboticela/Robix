# Robix

Robix is a language from Roboticela for programs that should feel as direct as C and as clear as a page you can read aloud. The names are PascalCase. Every value has a type. A function says what it receives, what it does, and what it returns before the body begins. You can run a file immediately with the interpreter, or compile it into a native executable.

A program starts in a package, imports what it needs, and enters through `Main`.

```robix
Package Robotics.Hello;

Import Robix.Core;

Public Function Main(Args<String> args) -> Int
Explain {
    Description: "Greet the operator and total the numbers from 1 through 5.";
    Input: args = "Command-line arguments";
    Processing: "Add the inclusive range and print the greeting and total.";
    Output: "Zero when the greeting has been printed.";
} {
    Var total = 0;
    For index In 1..=5 {
        total = total + index;
    }
    Print("Hello, Robix");
    Print(total);
    Return 0;
}
```

## The language

**Source and projects.** A `.rx` file is a Robix program or library. A `.rxp` file describes a project: its name, the files it includes, and whether the build is Debug or Release. A `.rxu` file is a Robix UI document. A `.rx3d` file is a Robix 3D scene.

**Names.** Keywords are PascalCase: `Function`, `If`, `For`, `Return`, `Class`. `ASM` stays in capitals. The names you invent, such as `total` or `robotName`, stay exactly as you wrote them.

**Types.** Strict code gives every variable, parameter, and return value one known type. `Let` and `Const` bind a value that does not change. `Var` binds one that does. `Int`, `String`, `Bool`, lists, arrays, and your own types are checked before the program runs. `Option` carries a value that may be absent. `Result` carries a success or a failure. `Any` can hold more than one kind of value, and you narrow it before you use it.

**Functions.** A function declares its parameters and its result, then an `Explain` block: description, input, processing, and output. After that comes the body. `Async Function` returns a `Task` when the work waits on something else. `Public` marks what other files may use.

**Flow.** `If`, `Else`, `While`, `For`, and `Match` decide what happens next. `For index In 1..=5` walks an inclusive range. `Match` compares a value with patterns and keeps the arms exhaustive.

**Types you define.** `Class` and `Struct` group fields and methods. `Interface` and `Trait` describe behavior another type can implement. `Enum` names a closed set of cases. A method belongs to its type, and a constructor builds a value of that type.

**Packages.** `Package` names the file. `Import` brings another package in, with `As` when you want a shorter name. The public surface of a package is the contract other code is allowed to call.

**Operators.** Arithmetic, comparison, and assignment use the usual signs. You can also define an operator of your own, including a symbol such as `⊗`, and give it a precedence.

**Tests.** A `Test` sits beside the function or method it checks. Each `Case` supplies input and says what must be true: the output, an assertion, or a condition. Running one function runs only that function's cases. Running a class runs the cases that belong to it.

**Two ways to execute.** The interpreter, `robix`, runs a file at once. The compiler, `robixc`, checks the same source more strictly and writes a native executable. A Release build is the program you ship.

## In the editor

Open a Robix file and the editor speaks the language with you. The features below are the ones this extension adds. Shortcuts are for Linux and Windows. On a Mac, use Cmd in place of Ctrl, and Option in place of Alt.

### Coloring

Keywords, types, functions, parameters, strings, numbers, comments, and operators each have their own color, chosen to match the Visual Studio editor people used in 2020. Keywords are blue. Control keywords such as `If` and `For` are purple. Types are teal. Functions are gold in the dark theme and brown in the light theme. Strings are rose. Numbers and comments are green.

Choose **Robix Dark** or **Robix Light** with the theme picker.

| Action | Shortcut |
| --- | --- |
| Choose a color theme | Ctrl+K, then Ctrl+T |

The cursor line is a clear frame: the fill is transparent, and a thin border sits on the top and bottom of the line. Indentation spaces are drawn as dots, and the indent guide you are inside is brighter than the others.

### Completion

Suggestions appear as you type. They include keywords, snippets, your functions, variables, parameters, types, imported packages, and names from the standard library. Typing `function` still offers `Function`, so the PascalCase form is what gets inserted.

| Action | Shortcut |
| --- | --- |
| Open suggestions | Ctrl+Space |
| Accept the selected suggestion | Enter or Tab |

Snippets expand a whole shape: a function with its `Explain` block, a class, a loop, a test, and the other forms you reach for often. Snippet suggestions stay at the top of the list.

### Signature help

When you type the opening parenthesis of a call, the parameter list appears above the cursor. The active parameter is highlighted, and it moves as you type commas.

| Action | Shortcut |
| --- | --- |
| Show the parameter list | Ctrl+Shift+Space |

### Hover

Hold the pointer over a keyword, function, variable, or type. A card shows its name, its signature, and the short explanation Robix knows for that word.

### Go to definition

Jump from a use of a name to the place it is declared, in this file or in another Robix file of the workspace.

| Action | Shortcut |
| --- | --- |
| Go to definition | F12 |
| Peek the definition without leaving the line | Alt+F12 |

### Find references

List every place a function, variable, parameter, or type is used. Rename then changes that name in all of those places. Keywords are not renamed. A name you chose, such as `myValue`, is.

| Action | Shortcut |
| --- | --- |
| Find all references | Shift+F12 |
| Rename the symbol | F2 |

### Keyword case

Finish a keyword and it becomes the real spelling. `if` becomes `If`, `function` becomes `Function`, and `asm` becomes `ASM`. The change happens when the word is complete, on a space, a parenthesis, or a brace, so `functionality` is never rewritten while you are still typing it. Your own names are left alone. This is on by default. Turn it off with `robix.autoCorrectKeywords`.

### Formatting

The formatter gives the file the shape Robix is written in. Each statement is on its own line. A block's body is indented four spaces. The opening brace stays with the header in a `.rx` file, and a project file keeps its braces on their own lines. Keywords are written in PascalCase. Comments stay where they are, and the names you declared are not recased.

Format on save is already on for Robix files.

| Action | Shortcut |
| --- | --- |
| Format the whole file | Shift+Alt+F |
| Format the selection | Ctrl+K, then Ctrl+F |
| Save, which also formats | Ctrl+S |

The indent width is `robix.format.indentSize`. It starts at 4.

### Outline, folding, and symbols

The outline lists the packages, functions, classes, methods, and fields in the file. Folding collapses a brace block or a long comment so the rest of the file stays in view. Sticky scroll keeps the function or type you are inside pinned at the top of the editor.

| Action | Shortcut |
| --- | --- |
| Go to a symbol in this file | Ctrl+Shift+O |
| Fold the block at the cursor | Ctrl+Shift+[ |
| Unfold | Ctrl+Shift+] |
| Fold everything | Ctrl+K, then Ctrl+0 |

### Problems

The file is checked as you type. A mismatched brace, a duplicated name, a break outside a loop, or an unknown name is marked in the editor and listed in the Problems panel. When the Robix interpreter is available, its check is added too. `Shift+F5` runs that check on the file you are editing.

| Action | Shortcut |
| --- | --- |
| Check the file | Shift+F5 |
| Open the Problems panel | Ctrl+Shift+M |
| Go to the next problem | F8 |

Checking can be turned off with `robix.diagnostics.enable`.

### Run

Run uses the interpreter. It is the way you see what the program prints.

| Action | Shortcut | What it does |
| --- | --- | --- |
| Run | F5 | Runs the open file with the interpreter. On a `.rxp` file, runs the project. |
| Compile | Ctrl+F5 | Compiles the file to a temporary executable and does not install it. On a project, builds the Debug configuration. |
| Release | Ctrl+Shift+F5 | Writes a production executable under `Build/Release`. On a project, publishes the Release configuration. |

The same three actions are on the editor title bar, in the right-click menu, and in the Command Palette as **Robix: Run**, **Robix: Compile**, and **Robix: Check**. The status bar shows **Robix** while a Robix file is active. Click it to run.

Extra arguments for a program started with Run go in `robix.run.arguments`.

The editor looks for `robix` and `robixc` on your PATH. A setting is used only when you fill in `robix.interpreterPath` or `robix.compilerPath`. Inside a Robix checkout, `Build/bootstrap` is the last place it looks.

GitHub Actions builds the toolchains, their updates, and the editor. The version is `VERSION` in the repository and is written to `Robix/VERSION.txt`. A tag such as `v1.2.3` uses that tag as the version. `robix-linux-x64.zip` and `robix-windows-x64.zip` are the full toolchains. Each one already contains LLVM's `llc` and `lld`, so nothing else has to be installed to compile. Unzip the archive for your system and add its `Robix/Bin` directory to PATH. On Linux the commands are `Robix` and `Robixc`. `Robix` runs a program. `Robixc` compiles every `.rx` file in a folder into one program: `llc` writes the machine code and `lld` links the compiled runtime in `Library/Runtime.lib`. The Windows archive has the same folders, `llc.exe`, `ld.lld.exe`, and the Windows runtime. `Update-[Version]-linux-x64.zip` and `Update-[Version]-windows-x64.zip` contain everything except LLVM. Unzip the matching update over an existing `Robix` folder to replace the tools, runtime, and packages. `Shared/Tools/Llvm` and `Library/Link` stay in place. `Roboticela.Robix.vsix` is the editor package at the same version: choose **Extensions: Install from VSIX** and select that file. A push to main publishes these files on the `latest` release. A version tag publishes a release for that version.

### Run one function or class

A function, a method, and a class have a single **Run** button above the name.

- On a function, it runs the tests named for that function and nothing else.
- On a method, it runs the tests named `Class.Method`.
- On a class, it runs every test that belongs to that class.

`PASS` or the failing case is written in the **Robix** output, and a failure is also marked on the name. If that symbol has no test cases, the run stops and says so. F5 still runs the whole file.

### File icons

| File | Icon |
| --- | --- |
| `.rx` | Robix source |
| `.rxp` | Robix project |
| `.rxu` | Robix UI |
| `.rx3d` | Robix 3D |

The extension itself uses the Robix language mark in the Extensions view.

### Comments and brackets

`//` comments a line. `/* */` comments a block. Brackets, braces, and quotes close themselves, and the matching bracket is highlighted when the cursor is beside one.

| Action | Shortcut |
| --- | --- |
| Comment or uncomment the line | Ctrl+/ |
| Comment or uncomment the block | Shift+Alt+A |

## License

Robix, including this editor, is licensed under the GNU Affero General Public License, version 3. The complete license is the `LICENSE` file in this package.

LLVM is not part of that license. When the toolchain archive includes `llc` and `lld`, their license is `Shared/Tools/Llvm/LLVM-LICENSE` in that archive. The C and C++ libraries used only to link a program have their own files, `Library/Link/LICENSE-GLIBC` and `Library/Link/LICENSE-GCC`.
