#!/usr/bin/env bash
set -euo pipefail

dest="${1:?clang destination is required}"
link="${2:?link library destination is required}"
mkdir -p "$dest/bin" "$dest/lib" "$link"

source_clang=$(readlink -f "$(command -v clang++)")
prefix=$(dirname "$(dirname "$source_clang")")
version=$(basename "$prefix"/lib/clang/*)
cp -a "$source_clang" "$dest/bin/clang"
ln -sfn clang "$dest/bin/clang++"
if [[ -x "$prefix/bin/llvm-ar" ]]; then
    cp -a "$prefix/bin/llvm-ar" "$dest/bin/llvm-ar"
fi
cp -a "$(readlink -f "$(command -v ld)")" "$dest/bin/ld"

resource="$dest/lib/clang/$version"
mkdir -p "$resource/lib/linux"
cp -a "$prefix/lib/clang/$version/include" "$resource/include"
cp -a \
    "$prefix/lib/clang/$version/lib/linux/clang_rt.crtbegin-x86_64.o" \
    "$prefix/lib/clang/$version/lib/linux/clang_rt.crtend-x86_64.o" \
    "$prefix/lib/clang/$version/lib/linux/libclang_rt.builtins-x86_64.a" \
    "$resource/lib/linux/"

declare -A seen=()
copy_deps() {
    local binary="$1"
    local line soname path real stored
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
        copy_deps "$dest/lib/$soname"
    done < <(ldd "$binary" || true)
}
copy_deps "$dest/bin/clang"
copy_deps "$dest/bin/ld"

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
