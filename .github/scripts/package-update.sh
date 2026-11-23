#!/usr/bin/env bash
set -euo pipefail

stage="${1:?Robix stage directory is required}"
platform="${2:?platform is required}"
root="$(cd "$(dirname "$0")/../.." && pwd)"
version="$(bash "$root/.github/scripts/version.sh")"

printf '%s\n' "$version" > "$stage/VERSION.txt"

temp="$root/dist/update-${platform}"
rm -rf "$temp"
mkdir -p "$temp"
cp -a "$stage" "$temp/Robix"
linker_src="$temp/Robix/Shared/Tools/Llvm/bin/robix-clang++.exe"
linker_copy=""
if [[ -f "$linker_src" ]]; then
    linker_copy="$temp/robix-clang++.exe"
    cp -a "$linker_src" "$linker_copy"
fi
rm -rf "$temp/Robix/Shared/Tools/Llvm" "$temp/Robix/Library/Link"
if [[ -n "$linker_copy" ]]; then
    mkdir -p "$temp/Robix/Shared/Tools/Llvm/bin"
    cp -a "$linker_copy" "$temp/Robix/Shared/Tools/Llvm/bin/robix-clang++.exe"
fi

cat > "$temp/Robix/UPDATE.txt" <<EOF
Robix update ${version} (${platform})

Unzip this archive over the folder that already contains Robix.
It replaces the interpreter, compiler, runtime, packages, shared files, README.md, and LICENSE.
It does not replace Shared/Tools/Llvm or Library/Link. Those stay in place, including their licenses.
The Windows update also adds Shared/Tools/Llvm/bin/robix-clang++.exe, which links a compiled program.

Linux:

    unzip -o Update-${version}-${platform}.zip -d /path/to/parent-of-Robix

Windows: expand this archive into the folder that already contains Robix.
EOF

archive="$root/dist/Update-${version}-${platform}.zip"
rm -f "$archive"
(
    cd "$temp"
    zip -r "$archive" Robix
)
file_list=$(unzip -l "$archive" | tr '\\' '/')
if [[ "$file_list" == *"Library/Link/"* ]]; then
    echo "error: ${archive} still contains link libraries" >&2
    exit 1
fi
llvm_extra=$(printf '%s\n' "$file_list" | grep 'Shared/Tools/Llvm/' | grep -v 'robix-clang++\.exe' | grep -v '/$' || true)
if [[ -n "$llvm_extra" ]]; then
    echo "error: ${archive} still contains LLVM files" >&2
    printf '%s\n' "$llvm_extra" >&2
    exit 1
fi
if [[ "$file_list" != *"Bin/"* ]]; then
    echo "error: ${archive} is missing the Robix files" >&2
    exit 1
fi
if [[ "$file_list" != *"Robix/README.md"* ]]; then
    echo "error: ${archive} is missing README.md" >&2
    exit 1
fi
if [[ "$file_list" != *"Robix/LICENSE"* ]]; then
    echo "error: ${archive} is missing LICENSE" >&2
    exit 1
fi
rm -rf "$temp"
echo "Packaged $archive"
