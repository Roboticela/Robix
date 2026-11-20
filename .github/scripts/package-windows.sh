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
        size=$(stat -c%s "$file")
    fi
    if (( size < minimum )); then
        curl -L --fail --retry 3 -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
}

fetch "$linux_url" "$linux_tar" 80000000
fetch "$windows_url" "$windows_zip" 180000000

rm -rf "$cache/llvm-mingw-linux" "$cache/llvm-mingw-windows"
mkdir -p "$cache/llvm-mingw-linux" "$cache/llvm-mingw-windows"
tar -xJf "$linux_tar" -C "$cache/llvm-mingw-linux"
unzip -q "$windows_zip" -d "$cache/llvm-mingw-windows"

toolchain=$(find "$cache/llvm-mingw-linux" -mindepth 1 -maxdepth 1 -type d | head -n 1)
cross="$toolchain/bin/x86_64-w64-mingw32-clang++"
cross_ar="$toolchain/bin/llvm-ar"
if [[ ! -x "$cross" || ! -x "$cross_ar" ]]; then
    echo "error: the Windows cross compiler was not found in the toolchain" >&2
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
find "$stage/Shared/Tools/Llvm/bin" -maxdepth 1 \( -name 'clang' -o -name 'clang.exe' -o -name 'clang++' -o -name 'clang++.exe' -o -name '*-clang' -o -name '*-clang.exe' -o -name '*-clang++' -o -name '*-clang++.exe' -o -name '*-g++' -o -name '*-g++.exe' -o -name '*-gcc' -o -name '*-gcc.exe' \) -delete
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
install -m 644 "$root/.github/packaging/clang++.ps1" "$stage/Shared/Tools/clang++.ps1"
install -m 644 "$root/.github/packaging/clang++.cmd" "$stage/Shared/Tools/clang++.cmd"
install -m 644 "$root/.github/packaging/Robixc.ps1" "$stage/Bin/Robixc.ps1"
install -m 644 "$root/.github/packaging/Robixc.cmd" "$stage/Bin/Robixc.cmd"

"$cross" -std=c++17 -O3 -c -o "$stage/Runtime/Runtime.o" "$root/Runtime/Runtime.cpp"
"$cross_ar" rcs "$stage/Library/Runtime.lib" "$stage/Runtime/Runtime.o"
file "$stage/Runtime/Runtime.o" | grep -E -q "COFF|PE32|Windows"

cat > "$stage/README.txt" <<'EOF'
Robix toolchain for Windows

Bin\Robixc.cmd            Compiles a folder of Robix files when Shared\Tools\Compiler.exe is present.
Shared\Tools\Llvm\bin     llc.exe generates machine code. ld.lld.exe links the program.
Library\Runtime.lib       Runtime library used when linking a program.
Runtime\Runtime.o         Compiled Windows runtime.
Packages\Builtin          Built-in package area.
Packages\Installed        Place installed packages here.
Shared                    Compiled files, grouped by area. UI files are in Shared\UI.

Add Bin to PATH. llc and lld are inside this zip. No separate compiler install is required.

The folders match the Linux toolchain. Robix.exe is not included yet: rebuilding the language compiler currently emits invalid LLVM, so there is no Windows interpreter binary.
EOF

archive="$root/dist/robix-windows-x64.zip"
rm -f "$archive"
(
    cd "$root/dist/windows-stage"
    zip -r "$archive" Robix
)
echo "Packaged $archive"
