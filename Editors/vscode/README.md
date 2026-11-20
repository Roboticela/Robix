# Robix for VS Code

Editing support for `.rx` source and `.rxp` projects.

## What you get

- Coloring for keywords, types, functions, strings, numbers, and comments. **Robix Dark** and **Robix Light** are flat themes made for this language. Pick one with the theme selector.
- Completion for keywords, snippets, your declarations, imported packages, and standard library names. Typing in any case still finds the PascalCase spelling.
- Signature help, hover, go to definition, find references, rename, outline, and folding.
- Checks from the Robix language service, plus `robix check` when the interpreter is available.
- A formatter that puts each statement on its own line, keeps blocks indented, and writes keywords in PascalCase. Format on save is on. Your own names are not recased.
- Keywords such as `function` or `if` become `Function` and `If` when you finish the word. A name like `myValue` stays as you wrote it.

## Running

| Action | Shortcut | What it does |
| --- | --- | --- |
| Run | F5 | Runs the file with the interpreter. This is the default. |
| Compile | Ctrl+F5 | Type-checks and writes a temporary executable. |
| Release | Ctrl+Shift+F5 | Writes a production executable under `Build/Release`. |
| Check | Shift+F5 | Checks the file without running it. |

On a `.rxp` file, Run executes the project, Compile builds it, and Release publishes the Release configuration.

The buttons are also on `Main` and on test declarations.

The tools are found in `Build/bootstrap`. Set `robix.interpreterPath` and `robix.compilerPath` if they live somewhere else.

## Install

From this folder:

```bash
cursor --install-extension Editors/vscode --force
```

Or use **Developer: Install Extension from Location** and choose `Editors/vscode`. Then reload the window.
