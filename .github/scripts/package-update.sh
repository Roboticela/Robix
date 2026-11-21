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
rm -rf "$temp/Robix/Shared/Tools/Llvm" "$temp/Robix/Library/Link"

cat > "$temp/Robix/UPDATE.txt" <<EOF
Robix update ${version} (${platform})

Unzip this archive over the folder that already contains Robix.
It replaces the interpreter, compiler, runtime, packages, shared files, README.md, and LICENSE.
It does not contain LLVM or the link libraries. Shared/Tools/Llvm, Shared/Tools/Llvm/LLVM-LICENSE, and Library/Link stay in place.

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
if [[ "$file_list" =~ (Shared/Tools/Llvm/|Library/Link/) ]]; then
    echo "error: ${archive} still contains LLVM files" >&2
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
