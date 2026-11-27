#!/usr/bin/env bash
# Lay out a Robix toolchain directory.
# Usage: stage-layout.sh <stage> <target> <robix> <robixc> [bootstrap]
# target is linux-x64, windows-arm64, and the other names the bootstrap knows.
set -euo pipefail

stage="${1:?stage directory is required}"
target="${2:?target is required}"
robix="${3:?Robix executable is required}"
robixc="${4:?Robixc executable is required}"
bootstrap="${5:-}"
root="$(cd "$(dirname "$0")/../.." && pwd)"

if [[ "$target" == windows-* ]]; then
    os="windows"
    unit="dll"
    kind="dll"
else
    os="linux"
    unit="o"
    kind="elf"
fi

rm -rf "$stage"
mkdir -p "$stage/Bin" "$stage/Packages/Builtin" "$stage/Packages/Installed" \
    "$stage/Shared" "$stage/Documentation" "$stage/Library"

if [[ "$os" == "windows" ]]; then
    cp -a "$robix" "$stage/Bin/Robix.exe"
    cp -a "$robixc" "$stage/Bin/Robixc.exe"
    if [[ -n "$bootstrap" && -f "$bootstrap" ]]; then
        cp -a "$bootstrap" "$stage/Bin/RobixBootstrap.exe"
    fi
    printf '%s\n' "kernel32.dll" "ExitProcess" "GetStdHandle" "WriteFile" > "$stage/Shared/kernel32.txt"
else
    install -m 755 "$robix" "$stage/Bin/Robix"
    install -m 755 "$robixc" "$stage/Bin/Robixc"
    if [[ -n "$bootstrap" && -f "$bootstrap" ]]; then
        install -m 755 "$bootstrap" "$stage/Bin/RobixBootstrap"
    fi
    install -m 644 "$root/Licenses/GCC-RUNTIME-LICENSE" "$stage/Shared/LICENSE-GCC"
    install -m 644 "$root/Licenses/GLIBC-LICENSE" "$stage/Shared/LICENSE-GLIBC"
fi

compile_unit() {
    local source="$1"
    local destination="$2"
    local symbol="$3"
    python3 "$root/.github/scripts/unit-object.py" "$kind" "$target" "$source" "$destination" "$symbol"
}

while IFS= read -r -d '' source; do
    base="$(basename "$source" .rx)"
    compile_unit "$source" "$stage/Library/${base}.${unit}" "$base"
done < <(find "$root" -path "$root/Packages" -prune -o -path "$root/Examples" -prune -o -path "$root/Build" -prune -o -path "$root/dist" -prune -o -path "$root/Editors" -prune -o -type f -name '*.rx' ! -name '*.test.rx' -print0)

for package_dir in "$root"/Packages/Robix.*; do
    [[ -d "$package_dir" ]] || continue
    package="$(basename "$package_dir")"
    dest="$stage/Packages/Builtin/$package"
    mkdir -p "$dest"
    source_file="$(find "$package_dir" -maxdepth 1 -type f -name '*.rx' ! -name '*.test.rx' -print -quit)"
    if [[ -z "$source_file" ]]; then
        echo "error: $package has no source file" >&2
        exit 1
    fi
    install -m 644 "$source_file" "$dest/${package}.rx"
    test_file="$(find "$package_dir" -maxdepth 1 -type f -name '*.test.rx' -print -quit || true)"
    if [[ -n "$test_file" ]]; then
        install -m 644 "$test_file" "$dest/${package}.tests.rx"
    fi
    compile_unit "$source_file" "$dest/${package}.${unit}" "$package"
done

bash "$root/.github/scripts/install-docs.sh" "$stage"
printf '%s\n' "$(bash "$root/.github/scripts/version.sh")" > "$stage/VERSION.txt"

if [[ ! -f "$stage/Bin/Robix" && ! -f "$stage/Bin/Robix.exe" ]]; then
    echo "error: Bin is missing Robix" >&2
    exit 1
fi
if [[ ! -f "$stage/Bin/Robixc" && ! -f "$stage/Bin/Robixc.exe" ]]; then
    echo "error: Bin is missing Robixc" >&2
    exit 1
fi
if [[ ! -d "$stage/Documentation" || ! -d "$stage/Packages/Installed" ]]; then
    echo "error: Documentation or Packages/Installed is missing" >&2
    exit 1
fi
if [[ ! -f "$stage/Packages/Builtin/Robix.Core/Robix.Core.rx" || ! -f "$stage/Packages/Builtin/Robix.Core/Robix.Core.${unit}" ]]; then
    echo "error: Robix.Core was not staged" >&2
    exit 1
fi
if [[ ! -f "$stage/Library/Lexer.${unit}" ]]; then
    echo "error: Library/Lexer.${unit} was not staged" >&2
    exit 1
fi
echo "staged $target at $stage"
