#!/usr/bin/env bash
# Build/ is gitignored, so a clean runner has no compiler. The previous
# Linux release is the seed. The host build then recompiles the current source.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
dest="$root/Build/bootstrap"
mkdir -p "$dest"

if [[ -x "$dest/robix-run.app" && -x "$dest/robixc.app" ]]; then
    exit 0
fi

repo="${GITHUB_REPOSITORY:-Roboticela/Robix}"
cache="$root/dist/.cache"
mkdir -p "$cache"
archive="$cache/robix-linux-x64.zip"
url="https://github.com/${repo}/releases/latest/download/robix-linux-x64.zip"
echo "downloading the Linux compiler seed from ${url}" >&2
curl -L --fail --retry 3 -o "$archive.part" "$url"
mv "$archive.part" "$archive"

extract="$cache/bootstrap-seed"
rm -rf "$extract"
mkdir -p "$extract"
unzip -q "$archive" "Robix/Bin/Robix" "Robix/Shared/Tools/Compiler" -d "$extract"

host="$extract/Robix/Bin/Robix"
compiler="$extract/Robix/Shared/Tools/Compiler"
if [[ ! -f "$host" ]]; then
    echo "error: the Linux release has no Robix/Bin/Robix" >&2
    exit 1
fi
if [[ ! -f "$compiler" ]]; then
    compiler="$host"
fi

sig=$(od -An -t x1 -N 4 "$host" | tr -d ' \n')
if [[ "$sig" != "7f454c46" ]]; then
    echo "error: the downloaded Robix binary is not an ELF" >&2
    exit 1
fi

cp -a "$host" "$dest/robix-run.app"
cp -a "$compiler" "$dest/robixc.app"
chmod 755 "$dest/robix-run.app" "$dest/robixc.app"
echo "seeded Build/bootstrap from the latest Linux release" >&2
