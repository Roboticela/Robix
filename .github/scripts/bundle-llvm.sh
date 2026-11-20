#!/usr/bin/env bash
set -euo pipefail

dest="${1:?LLVM destination is required}"
link="${2:?link library destination is required}"
mkdir -p "$dest/bin" "$dest/lib" "$link"

llc=$(readlink -f "$(command -v llc)")
if [[ -x /usr/bin/ld.lld ]]; then
    lld=$(readlink -f /usr/bin/ld.lld)
elif [[ -x /usr/lib/llvm-18/bin/ld.lld ]]; then
    lld=$(readlink -f /usr/lib/llvm-18/bin/ld.lld)
else
    root="$(cd "$(dirname "$0")/../.." && pwd)"
    cached=$(find "$root/dist/.cache/llvm-mingw-linux" -type f -name lld -path '*/bin/lld' 2>/dev/null | head -n 1 || true)
    if [[ -z "${cached}" || ! -x "${cached}" ]]; then
        echo "error: ld.lld was not found. Install the lld package on the build machine." >&2
        exit 1
    fi
    lld=$cached
fi

cp -a "$llc" "$dest/bin/llc"
cp -a "$lld" "$dest/bin/lld"
ln -sfn lld "$dest/bin/ld.lld"

declare -A seen=()
copy_deps() {
    local binary="$1"
    local hint="${2:-}"
    local line soname path real
    while IFS= read -r line; do
        [[ "$line" == *"=>"* ]] || continue
        soname=${line%% =>*}
        soname=${soname#$'\t'}
        soname=${soname##* }
        case "$soname" in
            linux-vdso.so.1|ld-linux-x86-64.so.2|libc.so.6|libm.so.6|libdl.so.2|librt.so.1|libpthread.so.0)
                continue
                ;;
        esac
        path=${line#*=> }
        path=${path%% (*}
        [[ -e "$path" ]] || continue
        if [[ -e "$dest/lib/$soname" || -L "$dest/lib/$soname" ]]; then
            continue
        fi
        real=$(readlink -f "$path")
        if [[ -n "${seen[$real]:-}" ]]; then
            ln -sfn "${seen[$real]}" "$dest/lib/$soname"
            continue
        fi
        seen[$real]=$soname
        cp -a "$real" "$dest/lib/$soname"
        copy_deps "$dest/lib/$soname" "$hint"
    done < <(LD_LIBRARY_PATH="${hint}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ldd "$binary" || true)
}
copy_deps "$dest/bin/llc" "$(dirname "$(dirname "$llc")")/lib"
copy_deps "$dest/bin/lld" "$(dirname "$(dirname "$lld")")/lib"

gcc=$(ls -d /usr/lib/gcc/x86_64-linux-gnu/* | sort | tail -n 1)
cp -a /usr/lib/x86_64-linux-gnu/Scrt1.o /usr/lib/x86_64-linux-gnu/crti.o /usr/lib/x86_64-linux-gnu/crtn.o "$link/"
cp -a "$gcc/crtbeginS.o" "$gcc/crtendS.o" "$gcc/libgcc.a" "$gcc/libgcc_eh.a" "$gcc/libstdc++.a" "$link/"
cp -a /usr/lib/x86_64-linux-gnu/libc_nonshared.a /lib/x86_64-linux-gnu/libc.so.6 /lib/x86_64-linux-gnu/libm.so.6 "$link/"
if [[ -f /lib/x86_64-linux-gnu/libmvec.so.1 ]]; then
    cp -a /lib/x86_64-linux-gnu/libmvec.so.1 "$link/"
fi
cat > "$link/libc.so" <<'EOF'
OUTPUT_FORMAT(elf64-x86-64)
GROUP ( libc.so.6 libc_nonshared.a )
EOF
cat > "$link/libm.so" <<'EOF'
OUTPUT_FORMAT(elf64-x86-64)
GROUP ( libm.so.6 )
EOF
cat > "$link/libgcc_s.so" <<'EOF'
OUTPUT_FORMAT(elf64-x86-64)
GROUP ( libgcc.a libgcc_eh.a )
EOF
