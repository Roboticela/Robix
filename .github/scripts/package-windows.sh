#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"

is_windows() {
    [[ "${OSTYPE:-}" == "msys"* || "${OSTYPE:-}" == "cygwin"* || "$(uname -s 2>/dev/null)" == *"MINGW"* || "$(uname -s 2>/dev/null)" == *"MSYS"* ]]
}

stage="$root/dist/windows-stage/Robix"
rm -rf "$root/dist/windows-stage"
mkdir -p "$stage/Bin" "$stage/Packages/Builtin" "$stage/Packages/Installed" \
    "$stage/Library" "$stage/Runtime" "$stage/Shared/Tools" \
    "$stage/Shared/Syntax" "$stage/Shared/Runtime" "$stage/Shared/Build" "$stage/Shared/UI" \
    "$stage/Packages/Builtin/Robix.Syntax" "$stage/Packages/Builtin/Robix.Runtime" \
    "$stage/Packages/Builtin/Robix.Build" "$stage/Packages/Builtin/Robix.Tools" \
    "$stage/Packages/Builtin/Robix.UI"

# The interpreter and the compiler engine are cross-compiled on Linux.
# Static libc++ keeps them from requiring llvm-mingw DLLs at startup.
windows_tools="$root/dist/windows-tools"
if [[ ! -f "$windows_tools/Robix.exe" ]]; then
    found=$(find "$root/dist" -name Robix.exe -type f -print -quit || true)
    if [[ -n "$found" ]]; then
        windows_tools="$(dirname "$found")"
    fi
fi
if [[ ! -f "$windows_tools/Robix.exe" || ! -f "$windows_tools/Compiler.exe" ]]; then
    echo "error: dist/windows-tools/Robix.exe and Compiler.exe are missing. Run .github/scripts/cross-compile-windows.sh on Linux first." >&2
    exit 1
fi
cp -a "$windows_tools/Robix.exe" "$stage/Bin/Robix.exe"
cp -a "$windows_tools/Compiler.exe" "$stage/Shared/Tools/Compiler.exe"
cp -a "$windows_tools/Compiler.exe" "$stage/Bin/Robixc.exe"

rm -f "$stage/Bin/"*.ps1 "$stage/Bin/"*.cmd

# Built-in packages
place_bin() {
    local source="$1"
    local area="$2"
    local package="$3"
    local name
    name="$(basename "$source")"
    if [[ ! -f "$source" ]]; then
        return 0
    fi
    install -m 644 "$source" "$stage/Shared/$area/$name"
    mkdir -p "$stage/Packages/Builtin/$package"
    install -m 644 "$source" "$stage/Packages/Builtin/$package/$name"
}

syntax=(
    Lexer.bin Parser.bin ParserExpressions.bin ParserStatements.bin
    Token.bin TokenKind.bin TypeChecker.bin NameChecker.bin
    Emitter.bin SyntaxCompiler.bin Diagnostic.bin
    Rxir.bin Passes.bin Rxasm.bin Elf.bin Pe.bin Target.bin
    AstNode.bin TextConvert.bin SyntaxCoverage.bin
)
for name in "${syntax[@]}"; do
    place_bin "$root/Build/Compiler/$name" Syntax Robix.Syntax
done
place_bin "$root/Build/Compiler/Interpreter.bin" Runtime Robix.Runtime
place_bin "$root/Build/Compiler/Bytecode.bin" Runtime Robix.Runtime
place_bin "$root/Build/Compiler/Machine.bin" Runtime Robix.Runtime
place_bin "$root/Build/Compiler/Ownership.bin" Runtime Robix.Runtime
place_bin "$root/Build/Compiler/Text.bin" Runtime Robix.Runtime
place_bin "$root/Build/Compiler/List.bin" Runtime Robix.Runtime
place_bin "$root/Build/Compiler/Map.bin" Runtime Robix.Runtime
place_bin "$root/Build/Compiler/Linux.bin" Runtime Robix.Runtime
place_bin "$root/Build/Compiler/Windows.bin" Runtime Robix.Runtime
place_bin "$root/Build/Compiler/ProjectSystem.bin" Build Robix.Build
place_bin "$root/Build/Compiler/Robixc.bin" Tools Robix.Tools

is_pe() {
    local sig
    sig=$(od -An -t x1 -N 2 "$1" | tr -d ' \n')
    [[ "$sig" == "4d5a" ]]
}

for required in "$stage/Bin/Robix.exe" "$stage/Bin/Robixc.exe" "$stage/Shared/Tools/Compiler.exe"; do
    if [[ ! -f "$required" ]] || ! is_pe "$required"; then
        echo "error: $required is not a Windows executable" >&2
        exit 1
    fi
done

if is_windows; then
    smoke="$root/dist/windows-smoke"
    rm -rf "$smoke"
    mkdir -p "$smoke"
    cp "$root/Examples/HelloRobot.rx" "$root/Examples/HelloRobot.test.rx" "$smoke/"
    cat > "$smoke/Extra.rx" <<'EOF'
Package Robotics.Extra;

Public Function Extra() -> Int
Explain {
    Description: "Return one.";
    Processing: "Return the value.";
    Output: "The value one.";
} {
    Return 1;
}
EOF
    cat > "$smoke/Extra.test.rx" <<'EOF'
Test Extra {
    Constraint: True;
    Processing: True;
    Output: True;
}
EOF
    set +e
    run_output=$("$stage/Bin/Robix.exe" run "$smoke/HelloRobot.rx" 2>&1)
    run_status=$?
    set -e
    printf '%s\n' "$run_output"
    if [[ "$run_status" -ne 0 || "$run_output" != *"Hello, Robix"* ]]; then
        echo "error: Robix.exe could not run HelloRobot.rx (exit ${run_status})" >&2
        exit 1
    fi
    "$stage/Bin/Robixc.exe" "$smoke/HelloRobot.rx" "$smoke/Hello.exe" --target windows-x64
    if [[ ! -f "$smoke/Hello.exe" ]]; then
        echo "error: Robixc.exe did not write Hello.exe" >&2
        exit 1
    fi
    set +e
    compiled_output=$("$smoke/Hello.exe" 2>&1)
    compiled_status=$?
    set -e
    printf '%s\n' "$compiled_output"
    if [[ "$compiled_status" -ne 0 || "$compiled_output" != *"Hello, Robix"* ]]; then
        echo "error: the program Robixc.exe compiled did not run (exit ${compiled_status})" >&2
        exit 1
    fi
    rm -rf "$smoke"
fi

bash "$root/.github/scripts/install-docs.sh" "$stage"
cp -f "$root/README.md" "$stage/README.md"
cp -f "$root/LICENSE" "$stage/LICENSE"
bash "$root/.github/scripts/package-update.sh" "$stage" windows-x64

archive="$root/dist/robix-windows-x64.zip"
rm -f "$archive"
(
    cd "$root/dist/windows-stage"
    zip -r "$archive" Robix
)

file_list=$(unzip -l "$archive" | tr '\\' '/')

for required in 'Robix/README.md' 'Robix/LICENSE' 'Robix/Bin/Robix.exe' 'Robix/Bin/Robixc.exe' 'Robix/Shared/Tools/Compiler.exe'; do
    if [[ "$file_list" != *"$required"* ]]; then
        echo "error: ${archive} is missing ${required}" >&2
        exit 1
    fi
done

if [[ "$file_list" =~ Robix/Bin/[^[:space:]/]+\.(ps1|cmd) ]]; then
    echo "error: ${archive} contains .ps1 or .cmd files in Bin/" >&2
    exit 1
fi

echo "Packaged $archive"
