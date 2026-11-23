#!/usr/bin/env bash
# Open the Arm console from its display list.
# The window is the C surface. The Robix interpreter is not started.
cd "$(dirname "$0")"
mkdir -p Build/rxu
sources=(
    Runtime/Raster/rxu_raster.c
    Runtime/Raster/rxu_font.c
    Runtime/Raster/rxu_raster_main.c
    Platform/Linux/surface.c
)
rebuild=0
if [[ ! -x Build/rxu-surface || ! -x Build/rxu-raster ]]; then
    rebuild=1
fi
for source in "${sources[@]}"; do
    if [[ $source -nt Build/rxu-surface || $source -nt Build/rxu-raster ]]; then
        rebuild=1
    fi
done
if [[ $rebuild -eq 1 ]]; then
    gcc -O2 -std=c11 -o ./Build/rxu-raster \
        Runtime/Raster/rxu_raster.c Runtime/Raster/rxu_font.c Runtime/Raster/rxu_raster_main.c || exit 1
    gcc -O2 -std=c11 -o ./Build/rxu-surface \
        Runtime/Raster/rxu_raster.c Runtime/Raster/rxu_font.c Platform/Linux/surface.c -ldl || exit 1
fi
list="Examples/UI/ArmConsole.list"
if [[ -f Build/rxu/arm-s1.list ]]; then
    list="Build/rxu/arm-s1.list"
fi
cp "$list" Build/rxu/show.list
exec ./Build/rxu-surface Build/rxu/show.list --input Build/rxu/input.txt
