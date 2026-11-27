#!/usr/bin/env bash
# Build and package one non-host architecture.
# ROBIX_TARGET is linux-arm64, linux-arm, linux-riscv64, linux-ppc64le,
# linux-s390x, linux-loongarch64, windows-arm64, or windows-x86.
# ROBIX_CXX is the cross compiler for that target.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
target="${ROBIX_TARGET:?ROBIX_TARGET is required}"
cxx="${ROBIX_CXX:?ROBIX_CXX is required}"

if [[ "$target" == windows-* ]]; then
    mingw_bin="$(bash "$root/.github/scripts/ensure-mingw.sh")"
    export PATH="$mingw_bin:${PATH}"
    export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}"
    toolchain="$(cd "$mingw_bin/.." && pwd)"
    export LD_LIBRARY_PATH="$toolchain/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
if ! command -v "$cxx" >/dev/null 2>&1 && [[ ! -x "$cxx" ]]; then
    echo "error: $cxx was not found" >&2
    exit 1
fi

bash "$root/.github/scripts/ensure-bootstrap.sh"
boot="$root/Build/bootstrap/robix-run.app"
compiler="$cxx"
if [[ "$target" == "linux-arm" ]]; then
    compiler="$root/Build/bootstrap/cxx-armhf.sh"
    cat > "$compiler" <<EOF
#!/bin/sh
exec "$cxx" "\$@" -latomic
EOF
    chmod 755 "$compiler"
fi

(
    cd "$root"
    ROBIX_CXX="$compiler" "$boot" Build --emit Robix --emit Robixc --target "$target"
)

bin_dir="$root/dist/$target"
if [[ "$target" == windows-* ]]; then
    bootstrap="$bin_dir/RobixBootstrap.exe"
    "$compiler" -std=c++20 -O2 -static -o "$bootstrap" "$root/Bootstrap/main.cpp"
    interpreter="$bin_dir/Robix.exe"
    engine="$bin_dir/Compiler.exe"
else
    bootstrap="$bin_dir/RobixBootstrap"
    "$compiler" -std=c++20 -O2 -pthread -o "$bootstrap" "$root/Bootstrap/main.cpp"
    chmod 755 "$bootstrap"
    interpreter="$bin_dir/Robix.app"
    engine="$bin_dir/Robixc.app"
fi
if [[ ! -f "$interpreter" || ! -f "$engine" || ! -f "$bootstrap" ]]; then
    echo "error: $target did not produce the interpreter, compiler, and bootstrap" >&2
    exit 1
fi

stage_root="$root/dist/stage-$target"
stage="$stage_root/Robix"
rm -rf "$stage_root"
mkdir -p "$stage/Bin" "$stage/Packages/Builtin" "$stage/Packages/Installed" \
    "$stage/Library" "$stage/Runtime" "$stage/Shared/Tools" \
    "$stage/Shared/Syntax" "$stage/Shared/Runtime" "$stage/Shared/Build" "$stage/Shared/UI" \
    "$stage/Packages/Builtin/Robix.Syntax" "$stage/Packages/Builtin/Robix.Runtime" \
    "$stage/Packages/Builtin/Robix.Build" "$stage/Packages/Builtin/Robix.Tools" \
    "$stage/Packages/Builtin/Robix.UI"

if [[ "$target" == windows-* ]]; then
    cp -a "$interpreter" "$stage/Bin/Robix.exe"
    cp -a "$engine" "$stage/Shared/Tools/Compiler.exe"
    cp -a "$engine" "$stage/Bin/Robixc.exe"
    cp -a "$bootstrap" "$stage/Bin/RobixBootstrap.exe"
else
    install -m 755 "$interpreter" "$stage/Bin/Robix"
    install -m 755 "$engine" "$stage/Shared/Tools/Compiler"
    install -m 755 "$root/.github/packaging/Robixc" "$stage/Bin/Robixc"
    install -m 755 "$bootstrap" "$stage/Bin/RobixBootstrap"
fi

bash "$root/.github/scripts/install-docs.sh" "$stage"
bash "$root/.github/scripts/package-update.sh" "$stage" "$target"

archive="$root/dist/robix-$target.zip"
rm -f "$archive"
(
    cd "$stage_root"
    zip -r "$archive" Robix
)
rm -rf "$stage_root"

file_list=$(unzip -l "$archive" | tr '\\' '/')
if [[ "$target" == windows-* ]]; then
    required=('Robix/Bin/Robix.exe' 'Robix/Bin/Robixc.exe' 'Robix/Bin/RobixBootstrap.exe' 'Robix/LICENSE')
else
    required=('Robix/Bin/Robix' 'Robix/Bin/Robixc' 'Robix/Bin/RobixBootstrap' 'Robix/Library/Link/LICENSE-GCC' 'Robix/LICENSE')
fi
for name in "${required[@]}"; do
    if [[ "$file_list" != *"$name"* ]]; then
        echo "error: ${archive} is missing ${name}" >&2
        exit 1
    fi
done
echo "Packaged $archive"
