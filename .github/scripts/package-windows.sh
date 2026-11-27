#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"

is_windows() {
    [[ "${OSTYPE:-}" == "msys"* || "${OSTYPE:-}" == "cygwin"* || "$(uname -s 2>/dev/null)" == *"MINGW"* || "$(uname -s 2>/dev/null)" == *"MSYS"* ]]
}

stage="$root/dist/windows-stage/Robix"
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
bootstrap=""
if [[ -f "$windows_tools/RobixBootstrap.exe" ]]; then
    bootstrap="$windows_tools/RobixBootstrap.exe"
fi
bash "$root/.github/scripts/stage-layout.sh" "$stage" windows-x64 \
    "$windows_tools/Robix.exe" \
    "$windows_tools/Compiler.exe" \
    "$bootstrap"

is_pe() {
    local sig
    sig=$(od -An -t x1 -N 2 "$1" | tr -d ' \n')
    [[ "$sig" == "4d5a" ]]
}

for required in "$stage/Bin/Robix.exe" "$stage/Bin/Robixc.exe"; do
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
    (
        cd "$smoke"
        "$stage/Bin/Robixc.exe" "HelloRobot.rx" "Hello.exe" --target windows-x64
    )
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

archive="$root/dist/robix-windows-x64.zip"
rm -f "$archive"
(
    cd "$root/dist/windows-stage"
    if command -v zip >/dev/null 2>&1; then
        zip -r "$archive" Robix
    else
        py="python3"
        if ! command -v python3 >/dev/null 2>&1; then
            py="python"
        fi
        if ! command -v "$py" >/dev/null 2>&1; then
            echo "error: zip and python are missing, so the Windows archive cannot be written" >&2
            exit 1
        fi
        "$py" - "$archive" <<'PY'
import os
import sys
import zipfile

archive = sys.argv[1]
with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as packed:
    for dirpath, _dirnames, filenames in os.walk("Robix"):
        relative = os.path.relpath(dirpath, ".")
        if relative != ".":
            packed.write(dirpath, relative.replace("\\", "/") + "/")
        for name in filenames:
            path = os.path.join(dirpath, name)
            packed.write(path, os.path.relpath(path, ".").replace("\\", "/"))
PY
    fi
)

if command -v unzip >/dev/null 2>&1; then
    file_list=$(unzip -l "$archive" | tr '\\' '/')
else
    py="python3"
    if ! command -v python3 >/dev/null 2>&1; then
        py="python"
    fi
    file_list=$("$py" - "$archive" <<'PY'
import sys
import zipfile

with zipfile.ZipFile(sys.argv[1]) as packed:
    for name in packed.namelist():
        print(name.replace("\\", "/"))
PY
    )
fi

for required in 'Robix/README.md' 'Robix/LICENSE' 'Robix/Bin/Robix.exe' 'Robix/Bin/Robixc.exe' 'Robix/Library/Lexer.dll' 'Robix/Packages/Builtin/Robix.Core/Robix.Core.rx' 'Robix/Packages/Builtin/Robix.Core/Robix.Core.dll' 'Robix/Shared/kernel32.txt' 'Robix/Documentation/'; do
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
