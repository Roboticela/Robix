#!/usr/bin/env bash
# Cross-compile the real Robix interpreter and compiler for Windows.
# The Linux bootstrap drives the build. A clang++ on PATH forwards each
# compile and link to the mingw toolchain and writes PE executables.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
cache="$root/dist/.cache"
tools="$root/dist/windows-tools"
mkdir -p "$cache" "$tools"

linux_tar="$cache/llvm-mingw-linux.tar.xz"
linux_url="https://github.com/mstorsjo/llvm-mingw/releases/download/20261117/llvm-mingw-20261117-ucrt-ubuntu-22.04-x86_64.tar.xz"

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

fetch "$linux_url" "$linux_tar" 80000000
if [[ ! -d "$cache/llvm-mingw-linux" ]]; then
    rm -rf "$cache/llvm-mingw-linux"
    mkdir -p "$cache/llvm-mingw-linux"
    tar -xJf "$linux_tar" -C "$cache/llvm-mingw-linux"
fi

toolchain=$(find "$cache/llvm-mingw-linux" -mindepth 1 -maxdepth 1 -type d -print -quit)
cxx="$toolchain/bin/x86_64-w64-mingw32-clang++"
if [[ ! -x "$cxx" ]]; then
    echo "error: the mingw compiler was not found at $cxx" >&2
    exit 1
fi
bash "$root/.github/scripts/ensure-bootstrap.sh"
if [[ ! -x "$root/Build/bootstrap/robix-run.app" ]]; then
    echo "error: Build/bootstrap/robix-run.app is missing" >&2
    exit 1
fi

export LD_LIBRARY_PATH="$toolchain/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$cxx" --version >/dev/null

work=$(mktemp -d)
list="$work/objects.txt"
wrap="$work/bin"
mkdir -p "$wrap"
: > "$list"

cat > "$wrap/clang++" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

cxx="${ROBIX_MINGW_CXX:?}"
list="${ROBIX_OBJECT_LIST:?}"
dest_dir="${ROBIX_WINDOWS_TOOLS:?}"

mode=""
out=""
ll=""
runtime=""
previous=""
for argument in "$@"; do
    if [[ "$previous" == "-o" ]]; then
        out=$argument
    fi
    if [[ "$argument" == "-c" ]]; then
        mode="compile"
    fi
    if [[ "$argument" == "-shared" ]]; then
        mode="shared"
    fi
    case "$argument" in
        *.ll) ll=$argument ;;
        *.cpp) runtime=$argument ;;
    esac
    previous=$argument
done

if [[ "$mode" == "compile" ]]; then
    if [[ -z "$out" || -z "$ll" ]]; then
        echo "error: the Robix compile step did not name an object and an IR file" >&2
        exit 1
    fi
    triple="x86_64-w64-windows-gnu"
    if grep -q '^target triple = ' "$ll"; then
        sed -i "s/^target triple = .*/target triple = \"$triple\"/" "$ll"
    else
        rewritten=$(mktemp)
        printf 'target triple = "%s"\n' "$triple" | cat - "$ll" > "$rewritten"
        mv "$rewritten" "$ll"
    fi
    "$cxx" --target="$triple" -O2 -c -o "$out" "$ll"
    printf '%s\n' "$out" >> "$list"
    exit 0
fi

if [[ "$mode" == "shared" ]]; then
    exit 0
fi

if [[ -z "$out" || -z "$runtime" ]]; then
    echo "error: the Robix link step was not recognized:" >&2
    printf ' %q' "$@" >&2
    echo >&2
    exit 1
fi
if [[ ! -s "$list" ]]; then
    echo "error: the Windows build produced no object files" >&2
    exit 1
fi

link_objs=()
while IFS= read -r object; do
    [[ -n "$object" ]] || continue
    base=$(basename "$object")
    if [[ "$out" == */Robix.app && "$base" == "Robixc.o" ]]; then
        continue
    fi
    if [[ "$out" == */Robixc.app && "$base" == "Robix.o" ]]; then
        continue
    fi
    link_objs+=("$object")
done < "$list"
if [[ ${#link_objs[@]} -eq 0 ]]; then
    echo "error: no object files were selected for $out" >&2
    exit 1
fi

target=""
if [[ "$out" == */Robix.app ]]; then
    target="$dest_dir/Robix.exe"
elif [[ "$out" == */Robixc.app ]]; then
    target="$dest_dir/Compiler.exe"
else
    echo "error: unexpected Robix output $out" >&2
    exit 1
fi

mkdir -p "$dest_dir"
echo "link $target" >&2
"$cxx" --target=x86_64-w64-windows-gnu -static -std=c++17 -O2 -o "$target" "${link_objs[@]}" "$runtime"
cp -f "$target" "$out"
EOF
chmod 755 "$wrap/clang++"

export ROBIX_MINGW_CXX="$cxx"
export ROBIX_OBJECT_LIST="$list"
export ROBIX_WINDOWS_TOOLS="$tools"
export PATH="$wrap:$PATH"

probe="$work/probe"
mkdir -p "$probe"
printf '%s\n' 'target triple = "x86_64-w64-windows-gnu"' 'define i32 @main() { ret i32 0 }' > "$probe/probe.ll"
"$cxx" -c -o "$probe/probe.o" "$probe/probe.ll"

backup="$work/Robix.rxp"
cp "$root/Robix.rxp" "$backup"
restore_project() {
    cp "$backup" "$root/Robix.rxp"
}
trap restore_project EXIT

build_language() {
    local label="$1"
    : > "$list"
    rm -f "$root/Build/temp"/*.o
    echo "cross-compiling $label" >&2
    (
        cd "$root"
        ./Build/bootstrap/robix-run.app Build --Configuration Development --NoCache
    )
}

build_language "Robix.exe"
sed 's/Name: "Robix";/Name: "Robixc";/' "$backup" > "$root/Robix.rxp"
build_language "Compiler.exe"

is_pe() {
    local sig
    sig=$(od -An -t x1 -N 2 "$1" | tr -d ' \n')
    [[ "$sig" == "4d5a" ]]
}

for required in "$tools/Robix.exe" "$tools/Compiler.exe"; do
    if [[ ! -f "$required" ]] || ! is_pe "$required"; then
        echo "error: $required is not a Windows executable" >&2
        exit 1
    fi
done

echo "Cross-compiled $tools/Robix.exe and $tools/Compiler.exe"
