#!/bin/bash
# Regenerates gfx/mapv.png, gfx/treev.png and gfx/fsvng.gif from a Release build.
# Windows (Git Bash) + ffmpeg on PATH. It opens fsvng windows on the desktop for ~15 s.
# Why each step is the way it is: docs/screenshots.md
set -euo pipefail
cd "$(dirname "$0")/.."
exe=build/src/Release/fsvng.exe
[ -x "$exe" ] || { echo "Build Release first: cmake --build build --config Release"; exit 1; }

# A tree anyone can show: the dependency sources the build already fetched, minus their .git
sample=build/sample
if [ ! -d "$sample" ]; then
    mkdir -p "$sample"
    for d in sdl2 imgui glm json googletest glad; do
        cp -r "build/_deps/$d-src" "$sample/$d"
        rm -rf "$sample/$d/.git"
    done
fi
work=build/capture
mkdir -p "$work"

# capture NAME MODE THEME SECONDS: run fsvng with its own config and record its window
capture() {
    local name=$1 mode=$2 theme=$3 secs=$4
    mkdir -p "$work/$name/fsvng"
    echo "{\"lastMode\":$mode,\"themeName\":\"$theme\",\"window\":{\"width\":1600,\"height\":1000}}" \
        > "$work/$name/fsvng/config.json"
    APPDATA="$(cygpath -w "$work/$name")" "$exe" --expand-all "$(cygpath -w "$sample")" >/dev/null 2>&1 &
    sleep 1.5
    # fsvng isn't DPI-aware: on a scaled display gdigrab gets the unscaled image in the top-left
    # corner of a bigger frame, so crop to the window's own 1600x1000
    ffmpeg -loglevel error -y -f gdigrab -draw_mouse 0 -framerate 30 \
        -i title="fsvng - 3D File System Visualizer" -t "$secs" \
        -vf "crop=1600:1000:0:0" -c:v libx264 -pix_fmt yuv420p -crf 16 "$work/$name.mp4"
    taskkill //F //IM fsvng.exe >/dev/null
}

capture mapv 1 classic 6       # MapV = 1
capture treev 2 synthwave 4    # TreeV = 2

ffmpeg -loglevel error -y -ss 5 -i "$work/mapv.mp4" -frames:v 1 gfx/mapv.png
ffmpeg -loglevel error -y -ss 0.75 -i "$work/treev.mp4" -frames:v 1 gfx/treev.png
# GIF: the viewport only, the ring growing and the swoop, stopping before the close-up
ffmpeg -loglevel error -y -i "$work/treev.mp4" -vf "trim=0:2.7,crop=1200:940:400:60,fps=15,\
scale=720:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=bayer:bayer_scale=4" \
    gfx/fsvng.gif
ls -l gfx
