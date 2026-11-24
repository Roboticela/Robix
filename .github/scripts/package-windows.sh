#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
cache="$root/dist/.cache"
mkdir -p "$cache"
linux_tar="$cache/llvm-mingw-linux.tar.xz"
windows_zip="$cache/llvm-mingw-windows.zip"
linux_url="https://github.com/mstorsjo/llvm-mingw/releases/download/20261117/llvm-mingw-20261117-ucrt-ubuntu-22.04-x86_64.tar.xz"
windows_url="https://github.com/mstorsjo/llvm-mingw/releases/download/20261117/llvm-mingw-20261117-ucrt-x86_64.zip"

fetch() {
    local url="$1"
    local file="$2"
    local minimum="$3"
    local size=0
    if [[ -f "$file" ]]; then
        size=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file" 2>/dev/null || wc -c < "$file" || echo 0)
    fi
    if (( size < minimum )); then
        curl -L --fail --retry 3 -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
}

is_windows() {
    [[ "${OSTYPE:-}" == "msys"* || "${OSTYPE:-}" == "cygwin"* || "$(uname -s 2>/dev/null)" == *"MINGW"* || "$(uname -s 2>/dev/null)" == *"MSYS"* ]]
}

fetch "$windows_url" "$windows_zip" 180000000

if [[ ! -d "$cache/llvm-mingw-windows" ]]; then
    rm -rf "$cache/llvm-mingw-windows"
    mkdir -p "$cache/llvm-mingw-windows"
    unzip -q "$windows_zip" -d "$cache/llvm-mingw-windows"
fi

if is_windows; then
    toolchain=$(find "$cache/llvm-mingw-windows" -mindepth 1 -maxdepth 1 -type d | head -n 1)
    cross="$toolchain/bin/clang++.exe"
    cross_ar="$toolchain/bin/llvm-ar.exe"
else
    fetch "$linux_url" "$linux_tar" 80000000
    if [[ ! -d "$cache/llvm-mingw-linux" ]]; then
        rm -rf "$cache/llvm-mingw-linux"
        mkdir -p "$cache/llvm-mingw-linux"
        tar -xJf "$linux_tar" -C "$cache/llvm-mingw-linux"
    fi
    toolchain=$(find "$cache/llvm-mingw-linux" -mindepth 1 -maxdepth 1 -type d | head -n 1)
    cross="$toolchain/bin/x86_64-w64-mingw32-clang++"
    cross_ar="$toolchain/bin/llvm-ar"
fi

if [[ ! -x "$cross" && ! -f "$cross" ]]; then
    echo "error: the Windows compiler ($cross) was not found in the toolchain" >&2
    exit 1
fi

stage="$root/dist/windows-stage/Robix"
rm -rf "$root/dist/windows-stage"
mkdir -p "$stage/Bin" "$stage/Packages/Builtin" "$stage/Packages/Installed" \
    "$stage/Library" "$stage/Runtime" "$stage/Shared/Tools" \
    "$stage/Shared/Syntax" "$stage/Shared/Runtime" "$stage/Shared/Build" "$stage/Shared/UI" \
    "$stage/Packages/Builtin/Robix.Syntax" "$stage/Packages/Builtin/Robix.Runtime" \
    "$stage/Packages/Builtin/Robix.Build" "$stage/Packages/Builtin/Robix.Tools" \
    "$stage/Packages/Builtin/Robix.UI"

windows_root=$(find "$cache/llvm-mingw-windows" -mindepth 1 -maxdepth 1 -type d | head -n 1)
cp -a "$windows_root"/. "$stage/Shared/Tools/Llvm/"
link_driver=$(find "$stage/Shared/Tools/Llvm/bin" -maxdepth 1 -type f \( -name 'clang++.exe' -o -name 'clang++' \) | head -n 1)
if [[ -z "$link_driver" ]]; then
    echo "error: clang++ was not found in the Windows LLVM archive" >&2
    exit 1
fi
saved_link="$cache/robix-clang++.exe"
cp -a "$link_driver" "$saved_link"
find "$stage/Shared/Tools/Llvm/bin" -maxdepth 1 \( -name 'clang' -o -name 'clang.exe' -o -name 'clang++' -o -name 'clang++.exe' -o -name '*-clang' -o -name '*-clang.exe' -o -name '*-clang++' -o -name '*-clang++.exe' -o -name '*-g++' -o -name '*-g++.exe' -o -name '*-gcc' -o -name '*-gcc.exe' \) -delete
cp -a "$saved_link" "$stage/Shared/Tools/Llvm/bin/robix-clang++.exe"

if [[ ! -x "$stage/Shared/Tools/Llvm/bin/llc.exe" && ! -f "$stage/Shared/Tools/Llvm/bin/llc.exe" ]]; then
    llvm_win="$cache/llvm-windows.tar.xz"
    fetch "https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/clang+llvm-18.1.8-x86_64-pc-windows-msvc.tar.xz" "$llvm_win" 300000000
    rm -rf "$cache/llvm-windows"
    mkdir -p "$cache/llvm-windows"
    tar -xJf "$llvm_win" -C "$cache/llvm-windows" --wildcards '*/bin/llc.exe' '*/bin/*.dll'
    llc_exe=$(find "$cache/llvm-windows" -type f -name llc.exe | head -n 1)
    if [[ -z "$llc_exe" ]]; then
        echo "error: llc.exe was not found in the Windows LLVM archive" >&2
        exit 1
    fi
    cp -a "$llc_exe" "$stage/Shared/Tools/Llvm/bin/llc.exe"
    cp -a "$(dirname "$llc_exe")"/*.dll "$stage/Shared/Tools/Llvm/bin/" 2>/dev/null || true
fi

# The interpreter and the compiler engine are cross-compiled on Linux.
# Static libc++ keeps them from requiring llvm-mingw DLLs at startup.
windows_tools="$root/dist/windows-tools"
if [[ ! -f "$windows_tools/Robix.exe" && -f "$windows_tools/windows-tools/Robix.exe" ]]; then
    windows_tools="$windows_tools/windows-tools"
fi
if [[ ! -f "$windows_tools/Robix.exe" || ! -f "$windows_tools/Compiler.exe" ]]; then
    echo "error: dist/windows-tools/Robix.exe and Compiler.exe are missing. Run .github/scripts/cross-compile-windows.sh on Linux first." >&2
    exit 1
fi
cp -a "$windows_tools/Robix.exe" "$stage/Bin/Robix.exe"
cp -a "$windows_tools/Compiler.exe" "$stage/Shared/Tools/Compiler.exe"
"$cross" -std=c++17 -O2 -static -o "$stage/Bin/Robixc.exe" "$root/.github/packaging/Robixc.cpp"
"$cross" -std=c++17 -O2 -static -o "$stage/Shared/Tools/clang++.exe" "$root/.github/packaging/clang++.cpp"

# Ensure NO .ps1 or .cmd files in Bin/
rm -f "$stage/Bin/"*.ps1 "$stage/Bin/"*.cmd

# Compile runtime library
"$cross" -std=c++17 -O3 -c -o "$stage/Runtime/Runtime.o" "$root/Runtime/Runtime.cpp"
"$cross_ar" rcs "$stage/Library/Runtime.lib" "$stage/Runtime/Runtime.o"

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
    NativeEmit.bin Emitter.bin SyntaxCompiler.bin Diagnostic.bin
    Rxir.bin Rxasm.bin Elf.bin
    AstNode.bin TextConvert.bin SyntaxCoverage.bin
)
for name in "${syntax[@]}"; do
    place_bin "$root/Build/Compiler/$name" Syntax Robix.Syntax
done
place_bin "$root/Build/Compiler/Interpreter.bin" Runtime Robix.Runtime
place_bin "$root/Build/Compiler/ProjectSystem.bin" Build Robix.Build
place_bin "$root/Build/Compiler/Robixc.bin" Tools Robix.Tools

is_pe() {
    local sig
    sig=$(od -An -t x1 -N 2 "$1" | tr -d ' \n')
    [[ "$sig" == "4d5a" ]]
}

for required in "$stage/Bin/Robix.exe" "$stage/Bin/Robixc.exe" "$stage/Shared/Tools/Compiler.exe" "$stage/Shared/Tools/clang++.exe" "$stage/Shared/Tools/Llvm/bin/robix-clang++.exe" "$stage/Shared/Tools/Llvm/bin/llc.exe"; do
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
    "$stage/Bin/Robixc.exe" "$smoke" "$smoke/Hello.exe"
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
mkdir -p "$stage/Shared/Tools/Llvm"
cp -f "$root/Licenses/LLVM-LICENSE" "$stage/Shared/Tools/Llvm/LLVM-LICENSE"

bash "$root/.github/scripts/package-update.sh" "$stage" windows-x64

archive="$root/dist/robix-windows-x64.zip"
rm -f "$archive"
(
    cd "$root/dist/windows-stage"
    zip -r "$archive" Robix
)

file_list=$(unzip -l "$archive" | tr '\\' '/')

for required in 'Robix/README.md' 'Robix/LICENSE' 'Robix/Shared/Tools/Llvm/LLVM-LICENSE' 'Robix/Bin/Robix.exe' 'Robix/Bin/Robixc.exe' 'Robix/Shared/Tools/Compiler.exe' 'Robix/Shared/Tools/clang++.exe' 'Robix/Shared/Tools/Llvm/bin/robix-clang++.exe'; do
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
