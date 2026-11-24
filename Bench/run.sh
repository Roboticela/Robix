#!/bin/sh
# Median of five runs for each Bench row. A missing compiler is skipped.
set -eu
cd "$(dirname "$0")"
rows="ScalarAdd FloatSum ArraySum StringBuild ListSum FieldLoop OneInterface ScopeAlloc ConstWork CompileSelf"

median_of() {
    printf '%s\n' "$1" "$2" "$3" "$4" "$5" | sort -n | awk 'NR==3 { print }'
}

time_one() {
    start=$(date +%s%N)
    "$@" >/dev/null
    end=$(date +%s%N)
    echo $(( (end - start) / 1000000 ))
}

for row in $rows; do
    echo "$row"
    if command -v clang >/dev/null 2>&1 && [ -f "$row.c" ]; then
        clang -O3 -o "$row.c.out" "$row.c"
        a=$(time_one "./$row.c.out")
        b=$(time_one "./$row.c.out")
        c=$(time_one "./$row.c.out")
        d=$(time_one "./$row.c.out")
        e=$(time_one "./$row.c.out")
        echo "  clang -O3 $(median_of "$a" "$b" "$c" "$d" "$e") ms"
    fi
    if command -v g++ >/dev/null 2>&1 && [ -f "$row.cpp" ]; then
        extra=""
        if [ -f "${row}Ops.cpp" ]; then extra="${row}Ops.cpp"; fi
        g++ -O3 -o "$row.cpp.out" "$row.cpp" $extra
        a=$(time_one "./$row.cpp.out")
        b=$(time_one "./$row.cpp.out")
        c=$(time_one "./$row.cpp.out")
        d=$(time_one "./$row.cpp.out")
        e=$(time_one "./$row.cpp.out")
        echo "  g++ -O3 $(median_of "$a" "$b" "$c" "$d" "$e") ms"
    fi
done
