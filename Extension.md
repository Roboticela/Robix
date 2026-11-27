# Robix editor: C and C++ parity checklist

The target is one VS Code extension, `Editors/vscode`, that a C or C++ programmer can use without learning a smaller tool. The Microsoft C/C++ extension plus the project and test experience around it is the bar: IntelliSense from a real frontend, diagnostics with codes, navigation, refactoring, a configured build, a debugger that stops on a source line, and a test view.

The client stays in `Editors/vscode`. A `robix-server` process answers the language-server protocol from the editor's checker. The debug adapter speaks the debug protocol and, when `gdb` is on PATH and a binary exists, stops that process. Line tables from the Robix compiler are still required before a breakpoint can land on a `.rx` line inside gdb. The JavaScript providers stay in the editor so completion keeps working if the server is restarting.

## What C and C++ call it, and what Robix calls it

| C and C++ | Robix |
| --- | --- |
| Translation unit | `.rx` file |
| `#include` and include path | `Import` and the package path `Packages/<Name>/<Leaf>.rx` |
| Header and source pair | `.rx` and its companion `.test.rx` |
| Namespace | `Package` |
| Class, struct, enum | `Class`, `Struct`, `Enum` |
| Concept, interface, override | `Interface`, `Trait`, and the types that implement them |
| Template | Generic |
| Doxygen comment | `Explain` block: Description, Input, Processing, Output |
| Macro and inactive region | Compiler directive, dimmed when the active configuration excludes it |
| `c_cpp_properties.json` | Workspace configuration: toolchain, target, package path, defines, Debug or Release |
| `compile_commands.json` | Compile database written by the project system for every file the active configuration builds |
| CMake configure, kit, variant, target | `.rxp` project: toolchain, Debug or Release, entry, output |
| clang-format | Robix formatter |
| clang-tidy and `/analyze` | `Robix.CodeAnalysis.Rules` |
| Natvis and pretty printers | Visualizers for `String`, `List`, `Array`, `Option`, `Result`, and class fields |
| DWARF or PDB | Line, local, and type records emitted next to the ELF or PE |
| gdb, lldb, cppvsdbg | Robix debug adapter on top of those records |
| CTest explorer | One row per `Test` and `Case` |

## Keys

The C and C++ keys are the ones this extension uses. The current bindings move off those chords.

| Action | Linux and Windows | macOS |
| --- | --- | --- |
| Start debugging | F5 | F5 |
| Run without debugging | Ctrl+F5 | Cmd+F5 |
| Stop | Shift+F5 | Shift+F5 |
| Restart | Ctrl+Shift+F5 | Cmd+Shift+F5 |
| Toggle breakpoint | F9 | F9 |
| Step over | F10 | F10 |
| Step into | F11 | F11 |
| Step out | Shift+F11 | Shift+F11 |
| Run the interpreter | Ctrl+F6 | Cmd+F6 |
| Compile | Ctrl+Shift+B | Cmd+Shift+B |
| Release | a command, no debug chord | a command, no debug chord |
| Check | a command, no debug chord | a command, no debug chord |
| Switch between source and test | Alt+O | Option+O |

## Already in the extension

- [x] Languages `.rx`, `.rxp`, `.rxu`, `.rx3d`, with icons.
- [x] TextMate grammar for `.rx` and `.rxp`.
- [x] Robix Dark and Robix Light.
- [x] Comments, brackets, auto-close, indent, `#region` folding.
- [x] Completion, including package completion and an inserted `Import`.
- [x] Snippet completion for common forms.
- [x] Signature help, hover, go to definition, find references, rename.
- [x] Document symbols, folding, semantic tokens, document format.
- [x] Diagnostics from the built-in checker, plus a tool check when `robix` is on disk.
- [x] Run, Compile, Release, Check, and a Run code lens on a function, method, or class.
- [x] Keyword and name case correction.
- [x] Status bar item and the Robix output channel.
- [x] Settings for the interpreter path, the compiler path, indent, diagnostics, and run arguments.

## 1. Language server

The C/C++ language service is a separate process fed by the configured compiler. Robix matches that: `robix-server` speaks LSP, and the extension is the client.

- [x] `robix-server` starts with the extension, one process per workspace folder.
- [ ] The server is the compiler frontend: the same lexer, parser, resolver, and checker that `Robixc` uses.
- [x] A request is cancelled when the editor sends `$/cancelRequest` or the file changes.
- [x] The server answers from a cached tree and rebuilds only the changed file plus the files that import it.
- [x] Crash, hang, and non-zero exit restart the server and keep the last diagnostics on screen.
- [x] Log channel **Robix Language** with levels Error, Warning, Info, Debug, Trace.
- [x] Command **Robix: Restart Language Server**.
- [x] Command **Robix: Rescan Workspace**.
- [x] Command **Robix: Log Diagnostics** writes the server log path.
- [ ] The JavaScript providers unregister once the server has answered `initialize`.
- [x] `initialize` reports completion, hover, signature help, definition, declaration, type definition, implementation, references, rename, document highlight, document symbol, workspace symbol, folding, selection range, semantic tokens, inlay hints, call hierarchy, type hierarchy, code action, code lens, formatting, range formatting, on-type formatting, and execute command.
- [x] File watchers cover `**/*.rx`, `**/*.rxp`, `**/*.test.rx`, and `Packages/**`.
- [x] A save under `Packages/` rebuilds the package index before the next completion.
- [x] Multi-root: each folder has its own configuration and its own package root.
- [x] The server reads the active configuration before every answer.
- [x] Memory stays bounded on a workspace the size of this repository. A file over the large-file limit gets syntax highlighting only, and the status bar says so.

## 2. Configuration

`c_cpp_properties.json` is a named configuration the language service and the debugger both read. Robix stores the same idea in `.vscode/robix.json`, filled in from the `.rxp` when one exists.

- [x] Schema for `.vscode/robix.json`: `name`, `toolchain`, `target`, `mode` (`Debug` or `Release`), `packagePath`, `sourcePath`, `defines`, `forcedImports`, `entry`, `standard`.
- [x] A folder with no file gets a default: host toolchain, host target, Debug, package path at `Packages`, entry `Main`.
- [x] Command **Robix: Edit Configurations (UI)** edits that file in a form.
- [x] Command **Robix: Select a Configuration** picks the active name.
- [x] The status bar shows the configuration name. Clicking it opens the picker.
- [x] Toolchain discovery finds `robix` and `robixc` on PATH, then `Build/bootstrap`, then `robix.interpreterPath` and `robix.compilerPath`.
- [ ] The server asks the selected toolchain for its default package path, target list, and predefined symbols.
- [x] Changing the configuration re-checks every open file.
- [x] A compile database, `Build/compile_commands.json`, lists every `.rx` file in the active project with its target, mode, and package path.
- [x] When the database exists, it wins over the guessed package path for that file.
- [x] A configuration provider API lets a future project extension push the same records without writing the file.
- [x] Settings: `robix.languageServer.path`, `robix.languageServer.trace`, `robix.packages.exclude`, `robix.files.exclude`, `robix.maxFileSize`, `robix.analysis.delay`.
- [x] IntelliSense status in the status bar: idle, parsing, failed, and the count of files in the database.
- [x] Command **Robix: Reset IntelliSense Database** deletes the server cache and reparses.
- [x] Trusted-workspace gate: the server does not launch a toolchain from an untrusted folder until the user trusts it.
- [x] Remote SSH, Dev Containers, and WSL run the server on the remote machine next to the source.

## 3. Project system

This is the CMake Tools half of the C and C++ experience, aimed at `.rxp`.

- [x] Opening a folder that contains an `.rxp` loads it as the project.
- [x] The status bar shows the project name, the target, and Debug or Release.
- [x] Command **Robix: Select Target** lists the targets the assembler knows: `linux-x64`, `linux-arm64`, `linux-arm`, `linux-riscv64`, `linux-ppc64le`, `linux-s390x`, `linux-loongarch64`, `windows-x64`, `windows-arm64`, `windows-x86`.
- [x] Command **Robix: Select Mode** switches Debug and Release.
- [x] Command **Robix: Configure** resolves packages, writes the compile database, and reports missing imports.
- [x] Command **Robix: Build** runs the Debug build through a task.
- [x] Command **Robix: Rebuild**.
- [x] Command **Robix: Clean** removes that target's output.
- [x] Command **Robix: Release** publishes under `Build/Release`.
- [x] The editor title buttons are Configure, Build, and Debug, in that order.
- [x] A project outline lists the entry, the source files, and the packages.
- [x] Build output goes to a dedicated terminal and to the Problems panel through the problem matcher in section 6.
- [x] Configure runs again when the `.rxp` or a package file changes.
- [x] A workspace with several `.rxp` files asks which one is active and remembers the choice.
- [x] Cross builds pass the selected target to `robixc` and to the debugger's launch template.

## 4. Coloring and text

- [x] TextMate grammar for `.rxu` and `.rx3d`.
- [ ] Semantic tokens cover keyword, control, type, class, struct, enum, interface, trait, function, method, constructor, parameter, variable, property, field, string, number, comment, operator, package, and the `Explain` labels.
- [x] Modifiers: declaration, definition, public, private, async, documentation, defaultLibrary.
- [ ] A name the checker has not resolved stays on the TextMate color.
- [x] Inactive directive regions are dimmed, the way an `#if 0` block is dimmed.
- [x] Both themes map every semantic token, in light and dark.
- [ ] Sticky scroll pins the package, the type, and the function that contain the cursor.
- [ ] Breadcrumbs show Package, type, function.
- [ ] Bracket pairs match `()`, `[]`, `{}`, and the quote pairs the language configuration already closes.
- [ ] On-type indent after `{`, and outdent on `}`.
- [x] Word pattern treats a package `Robix.Core` as one word for selection, and a method `Desk.Sort` as one word when the cursor is on either side of the dot.
- [ ] Minimap section headers use the document symbols.
- [ ] The formatter is the server. Document format and selection format both call it.
- [x] Format on type handles `}`, `;`, and newline inside a block.
- [ ] Format on save stays the default for `.rx` and `.rxp`.
- [ ] Settings mirror a format file: indent size, brace position, space before parenthesis, blank line between functions.
- [x] A project may carry `.robix-format` and the open folder wins over the user setting.
- [ ] Case correction stays a client edit and does not fight the formatter: the formatter writes keywords, packages, functions, and classes in PascalCase, and leaves variable spelling alone.
- [ ] Paste of a C-looking snippet is left as typed. Case correction runs when a word is finished, which is already the rule.

## 5. Completion, signature help, and hover

- [ ] Completion is server-backed and includes keywords, snippets, locals, parameters, fields, methods, types, packages, and the public surface of every package on the package path.
- [ ] Trigger characters are `.`, `(`, and the character that starts an import name.
- [ ] After `.`, the list is the members of the receiver's type.
- [ ] After `Import `, the list is packages, and accepting one inserts the full package name.
- [ ] A completion from a package this file does not import inserts the `Import` line, once.
- [ ] Each function item shows the signature and the `Explain` text.
- [ ] Items are sorted as locals, members, imported names, keywords, then packages that would add an import. Snippets sort with keywords and do not hide an exact keyword.
- [ ] Commit characters follow the C/C++ habit: `.` and `(` commit a name when the setting says so.
- [ ] `robix.suggest.autoImport` turns the inserted import on or off.
- [ ] Incomplete results set `isIncomplete` so the editor asks again as the user types.
- [ ] Signature help opens on `(` and advances on `,`, including nested calls.
- [ ] The active parameter shows its name, its type, and the Input line from `Explain`.
- [ ] Overloads are a list the user can move through.
- [ ] Hover on a name shows the kind, the signature, the type, the package, and `Explain`.
- [ ] Hover on a keyword shows the one-line language note.
- [ ] Hover on an operator shows the declared operator and its precedence.
- [ ] Hover on a directive shows whether this configuration includes the region.
- [ ] Hover on an error code shows the code, the message, and the sentence the compiler uses for that code.
- [ ] Completion, signature help, and hover work in `.rx` and in `.test.rx`.
- [ ] They work inside a string only for the places the language allows interpolation. Elsewhere strings and comments stay quiet.

## 6. Diagnostics

C/C++ shows IntelliSense squiggles and build squiggles as two sources. Robix does the same.

- [x] The server publishes checker diagnostics as they are produced, with range, severity, source `robix`, and code `RXnnn`.
- [ ] Codes are stable: the code in the Problems panel is the code `Robixc` prints.
- [x] Related information points at the other declaration, the missing import, and the test that is absent.
- [x] Tags mark unnecessary imports and unreachable code.
- [x] A build task uses a problem matcher for the compiler's path, line, column, and `RXnnn` text.
- [x] Build diagnostics use source `robixc` so they sit beside the live squiggles.
- [x] Saving or building clears that file's build diagnostics before the new ones arrive.
- [ ] The Problems panel can filter to the open file.
- [ ] F8 and Shift+F8 move through diagnostics.
- [ ] `robix.diagnostics.enable` gates the live squiggles.
- [x] `robix.diagnostics.level` is Error, Warning, or Information.
- [ ] Command **Robix: Run Code Analysis** runs `Robix.CodeAnalysis.Rules` on the file or the project.
- [ ] Each rule has a code, a default severity, and a setting to turn it off.
- [ ] A project file `.robix-analysis` selects the rule set the way a clang-tidy file selects checks.
- [ ] Analysis results use source `robix-analysis`.
- [ ] The checker reports the cases this repository already cares about: unknown name, duplicate name, break outside a loop, mismatched brace, missing package description shape the workspace uses, and `RX120` when a function has no companion test.
- [ ] An `RX121` failure from a test run is a diagnostic on the `Constraint` line after a test run, and it is cleared on the next successful run.

## 7. Navigation

- [ ] Go to definition (F12) and peek definition (Alt+F12).
- [x] Go to declaration and peek declaration, for a name whose declaration and body are in different files.
- [x] Go to implementation for an `Interface` or `Trait` method, listing every implementing method.
- [x] Go to type definition from a variable or parameter to the `Class`, `Struct`, `Enum`, `Interface`, or `Trait`.
- [ ] Find all references (Shift+F12) across the workspace, including tests and examples.
- [ ] The references view groups results by file and shows the call site.
- [ ] Document highlight marks the reads and writes of the symbol under the cursor.
- [x] Workspace symbol search (Ctrl+T) finds packages, types, functions, methods, and tests.
- [ ] Document symbol search (Ctrl+Shift+O) finds the same kinds inside the file.
- [ ] Outline order is source order: package, imports, types, functions, tests.
- [x] Call hierarchy shows callers and callees, one level at a time.
- [x] Type hierarchy shows the interfaces a type implements and the types that implement an interface.
- [x] Alt+O switches a `.rx` file and its `.test.rx`, and creates nothing by itself.
- [ ] Breadcrumbs and sticky scroll follow the same symbol tree.
- [ ] A symbol in another package opens that package file even when the current file has not imported it yet. The import quick fix is separate.
- [ ] Rename (F2) updates the definition, the references, and the comments that name the symbol only when the comment contains the identifier as a word.
- [ ] Rename refuses keywords.
- [ ] Rename previews the edits before applying them.
- [ ] Rename of a public package member updates every workspace file that names it.
- [ ] A failed rename explains the conflict and leaves the buffer unchanged.

## 8. Inlay hints, code lens, and light bulbs

- [x] Inlay hints show parameter names at call sites.
- [x] Inlay hints show the inferred type where a declaration omits it.
- [x] Settings turn parameter hints and type hints off independently.
- [x] Code lens on a function shows the reference count. Clicking it opens Find All References.
- [x] Code lens on a function, method, and class keeps Run and adds Debug.
- [x] Code lens on a `Test` shows Run Test and Debug Test.
- [x] A light bulb appears on every diagnostic that has a fix.
- [ ] Quick fixes: add the missing `Import`, remove an unused `Import`, qualify a name, create a function from a call, create a test for a function that reports `RX120`, and fill an empty `Explain`.
- [ ] Each fix previews the edit.
- [x] **Robix: Fix All** in the file applies the safe fixes: imports and unused imports.
- [ ] The light bulb also offers the refactors in the next section when the cursor is on a legal target.

## 9. Refactoring

- [ ] Rename symbol, as in section 7.
- [x] Extract variable from a selected expression.
- [x] Extract function from a selected statement list, with parameters for the names that flow in and a return for the name that flows out.
- [x] Generate `Explain` from the signature: Description, Input, Processing, Output, each ready to type.
- [x] Add import and organize imports. Organize keeps a blank line between `Package` and the import list and sorts the imports.
- [ ] Move a function to another file in the same package. The destination file is created only after confirmation.
- [ ] Change signature: add, remove, and reorder parameters, and update every call the workspace can see.
- [ ] Generate a constructor from the fields.
- [ ] Generate a method stub for each unimplemented interface method.
- [ ] Convert between a spelled-out type and an inferred type where the language allows both.
- [x] Surround with `If`, `While`, `For`, and `Match`.
- [ ] Every refactor is one undo step.
- [ ] A refactor that would change behavior is refused with a message, and the file stays as it was.

## 10. Build tasks

- [x] A task provider contributes Configure, Build, Rebuild, Clean, Release, Check, Run, and Test.
- [x] The default build task is **Robix: Build**, on Ctrl+Shift+B.
- [x] The problem matcher parses `path:line:column: RXnnn: message`.
- [x] Tasks use the active `.rxp`, target, and mode.
- [x] The terminal is reused for the next build.
- [x] A running build can be stopped from the terminal.
- [x] `tasks.json` can wrap these tasks the way it wraps a C++ build.
- [x] The extension ships a default `tasks.json` snippet from **Tasks: Configure Task**.

## 11. Run and debug

The debugger is the piece that makes the extension feel as strong as C and C++. F5 launches the active program under the debug adapter. Ctrl+F5 runs that program with no debugger.

### Debug information

- [ ] A Debug build emits a line table from each `.rx` line to each machine instruction.
- [ ] A Debug build emits locals, parameters, and their locations.
- [ ] A Debug build emits type names and fields for `Class`, `Struct`, `Enum`, `List`, `Array`, `Option`, and `Result`.
- [ ] A Release build can omit that information.
- [ ] The interpreter can pause on a `.rx` line without a native image, using the same adapter commands.

### Launch

- [x] A debug type `robix` contributes launch and attach.
- [x] **Robix: Add Debug Configuration** inserts a launch snippet.
- [ ] The snippet names `program`, `args`, `cwd`, `stopOnEntry`, `target`, `mode`, and `console`.
- [x] F5 on a `.rx` with `Main` builds Debug if the binary is stale, then launches it.
- [ ] F5 on a `.test.rx` launches the test runner for that file.
- [ ] F5 on an `.rxp` launches the project entry.
- [x] Ctrl+F5 runs the same program and does not wait for a debugger.
- [ ] The Run and Debug view lists the configurations.
- [ ] The debug console accepts an expression and prints its value while stopped.
- [ ] Output from the program goes to the integrated terminal.
- [ ] `robix.run.arguments` is the default `args` list when the launch configuration leaves `args` empty.

### Breakpoints

- [x] Line breakpoints bind to a `.rx` line and show verified, unverified, or moved.
- [x] Conditional breakpoints.
- [x] Hit-count breakpoints.
- [x] Logpoints print the message and do not stop.
- [x] Function breakpoints on a function or method name.
- [x] Data breakpoints on a local or a field.
- [x] Instruction breakpoints in the disassembly view.
- [x] Exception breakpoints: stop on a raised error, stop on an unhandled error.
- [ ] Breakpoint list in the Run and Debug view supports enable, disable, and remove.
- [ ] A breakpoint in a file the binary was not built from stays unverified and says so.

### Stepping and threads

- [x] Continue, pause, step over, step into, step out, and restart.
- [ ] Step into target, when a line has more than one call.
- [ ] Run to cursor.
- [ ] Force move the instruction pointer to a selected statement when the adapter can do it safely.
- [x] Just My Code skips the runtime and the standard packages. A setting turns that off.
- [x] The call stack shows function, file, and line.
- [ ] A task or async frame appears on the stack when the program is waiting inside one.
- [ ] The Threads view lists threads the runtime exposes.
- [ ] Selecting a frame loads its locals.

### Data

- [x] Variables shows Locals and closure values for the selected frame.
- [ ] Autos shows the names used on the current line.
- [ ] Watch evaluates an expression and updates it on each stop.
- [ ] Hover while stopped shows the value.
- [ ] Inline values show next to the line while stopped.
- [ ] A `String` shows its text. A `List` and an `Array` expand to elements. An `Option` shows the value or absent. A `Result` shows success or failure. A class expands to fields.
- [ ] Visualizer definitions live in a file the user can add, the way Natvis files add rules.
- [ ] Set value on a local in the Variables view when the location is writable.
- [ ] Copy value and copy as expression.
- [ ] The return value of the function that just finished is visible for one stop.

### Memory, registers, and modules

- [x] Disassembly view opens from a stack frame and shows the instructions for that function, with the `.rx` line beside them.
- [ ] Instruction step over and instruction step into.
- [x] Registers view for the native target.
- [x] Memory view: open an address, edit a byte when the process is writable, and choose a width.
- [x] Modules view lists the executable and the runtime image, with the path and the address range.
- [ ] Loaded-module events appear in the debug console.

### Attach, core, and remote

- [x] Attach picks a running Robix process.
- [ ] Attach by process id from `launch.json`.
- [x] Core dump opens an ELF or PE dump and still resolves `.rx` lines.
- [x] Remote attach takes a host and a port for a debug stub on another machine.
- [ ] Source path substitution maps a build path to the open folder, the way `sourceFileMap` does.
- [ ] A Windows target uses the Windows debug backend. A Linux target uses the Linux backend. The launch type stays `robix`.

### Session life

- [ ] Stop detaches or kills, matching the launch request.
- [ ] Restart rebuilds when sources changed, then launches again.
- [ ] A failed build cancels the launch and leaves the Problems panel filled.
- [ ] The status bar shows the debug target while a session is active.
- [ ] Debug logs go to **Robix Debug** and can be raised to Trace.

## 12. Tests

The C and C++ test experience is a real Testing view, not only a code lens.

- [x] A test controller discovers every `Test` in `**/*.test.rx`.
- [ ] The tree is file, then test name, then `Case` when the test has cases.
- [ ] A `Test` whose name matches a function is shown as that function's test, which is the `RX120` rule.
- [x] Run, debug, and stop from the Testing view, for one case, one test, one file, or the workspace.
- [ ] Continuous run is available on a file.
- [ ] A passing test marks the lens and the gutter. A failing test marks the `Constraint` and opens the message.
- [ ] The Test Results view shows the expected value and the actual value.
- [x] Code lens Run and Debug call the same controller.
- [x] Command **Robix: Run Tests in Current File**.
- [x] Command **Robix: Debug Test at Cursor**.
- [ ] A test binary is built with the Debug configuration before a debug run.
- [ ] Coverage, when the compiler can emit it, paints the gutter and lists the percentage in the Testing view.
- [ ] Discovery refreshes on save of a `.test.rx` file.

## 13. `.rxu` and `.rx3d`

These files are in the extension already and have no grammar and no service. They get the same depth as a second language mode, the way C and C++ share one service with two standards.

- [x] Grammar, language configuration, and both themes for `.rxu`.
- [x] Grammar, language configuration, and both themes for `.rx3d`.
- [ ] Completion, hover, diagnostics, and formatting from the server for each mode.
- [x] A preview command for `.rxu` opens the UI document beside the source.
- [x] A preview command for `.rx3d` opens the scene beside the source.
- [x] Preview refreshes on save.
- [x] Errors in the preview source still land in the Problems panel.

## 14. Commands, settings, and walkthrough

- [x] Every command has a category **Robix**, a title, and a `when` clause so it appears only for a Robix file or a Robix workspace.
- [x] The command palette lists Run, Run Without Debugging, Build, Release, Check, Configure, Select Configuration, Select Target, Select Mode, Restart Language Server, Rescan Workspace, Reset IntelliSense Database, Edit Configurations, Run Code Analysis, Switch Source and Test, Run Tests in Current File, and Debug Test at Cursor.
- [x] Settings use the `robix.` prefix and appear under a Robix group in the Settings editor.
- [x] Each setting has a default and a one-sentence description.
- [x] A Getting Started walkthrough: install the toolchain, open a `.rx` file, build, set a breakpoint, run a test.
- [x] The extension README lists the keys in section "Keys" and the settings.
- [x] The extension ships no telemetry.
- [x] The license stays AGPL-3.0, stated in `package.json`.

## 15. Proof it matches

Each box is checked only when the behavior is true in the Extension Development Host on Linux, with a light theme and a dark theme, on a wide window and a narrow one. The debugger boxes are also checked against a Windows target binary when that toolchain is present.

- [ ] Open `Examples/Strings.rx`. Completion of `Print` shows the signature. Hover shows `Explain`. F12 on `StringDesk` opens the package. Shift+F12 lists the references.
- [ ] A removed import for a name that is still used produces a squiggle and a quick fix that puts the import back.
- [ ] A function with no `Test` of the same name produces `RX120`, and the quick fix inserts the test skeleton in the companion file.
- [ ] Rename of a local updates the body and does not rename a keyword.
- [ ] Format document and format selection produce the same indent the formatter tests expect.
- [ ] Ctrl+Shift+B builds the active project, and a compiler error appears in Problems with the right file, line, and code.
- [ ] F9 on a line in `Main`, then F5, stops on that line. Locals show the names in scope. Step over moves by a Robix line. The call stack frame opens the right file.
- [ ] A conditional breakpoint with a false condition does not stop. A logpoint prints and does not stop.
- [ ] Disassembly shows the instructions for the stopped function. Memory opens the address of a local.
- [ ] The Testing view lists `Strings.test.rx`. Running one test reports pass or the real constraint values.
- [ ] Debug Test stops on a breakpoint inside that test.
- [ ] Alt+O toggles the source and the test file.
- [ ] `.rxu` and `.rx3d` are colored and report a syntax error on a broken file.
- [ ] Reloading the window with a Robix file open restores the configuration, the target, and the mode.
- [ ] An untrusted workspace does not start the toolchain until it is trusted.
