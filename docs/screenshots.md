# Screenshots and the GIF

`gfx/mapv.png`, `gfx/treev.png` and `gfx/fsvng.gif` come from
[tools/capture-screenshots.sh](../tools/capture-screenshots.sh). Re-run it whenever the look
changes:

```
cmake --build build --config Release
bash tools/capture-screenshots.sh
```

It needs Windows (Git Bash) and ffmpeg on `PATH`, and opens two fsvng windows on the desktop for
about 15 seconds. Look at the output before committing; see "Timing" below.

## What it shows, and why

**The folder** is `build/sample`: the sources of SDL2, Dear ImGui, GLM, nlohmann/json, glad and
GoogleTest, copied from `build/_deps` without their `.git`. It is 825 folders and 5,309 files,
big enough to look like something, and it shows nobody's personal files. The previous
screenshots showed a real `C:\AMD` folder, file names and all.

**Each run gets its own config.** fsvng reads `%APPDATA%\fsvng\config.json`, so the script points
`APPDATA` at a fresh folder holding the mode (`lastMode`: 1 = MapV, 2 = TreeV), the theme and a
1600x1000 window. The view is set without clicking anything, and your own config is untouched.

**`--expand-all`** grows the whole tree right after the scan. Without it, only the top level
shows: six flat boxes.

## Hard-won details

**Crop to 1600x1000 from the top left.** fsvng isn't DPI-aware. On a display scaled to 200%,
`gdigrab -i title=...` returns a 3200x2000 frame with fsvng's unscaled 1600x1000 image in the
top-left quarter and black elsewhere (with the cursor drawn at full scale on top). Cropping gives
sharp native pixels. At 100% the frame is already 1600x1000, and the crop changes nothing.

**`-draw_mouse 0`**, or the cursor shows up wherever it happens to be on the desktop.

**Kill, don't close.** `taskkill /F` ends fsvng without its shutdown save, so the throwaway
config stays as written.

**Timing.** MapV finishes expanding before the first recorded frame, so its still comes from
5 s in, when everything has settled. TreeV is all motion: the ring grows outward, the core radius
jumps as it fills, and then the camera swoops down to the root (see [treev.md](treev.md)). The
still is taken at 0.75 s, mid-growth, and the GIF stops at 2.7 s, before the final close-up. How
far the growth has got at a given second depends on how long the scan took, so check
`gfx/treev.png` and nudge the `-ss 0.75` if it caught the tree too early (labels floating ahead
of their platforms) or too late (already swooping).

**Why not offstage.** The standing rule is to run windowed programs on an
[offstage](https://github.com/sp00nznet/offstage) virtual monitor. On 2026-10-19 it failed here
at the console with the driver installed and reporting OK:

```
offstage: the monitor was added but never showed up in the display configuration
```

Same result with `hold`, and outside the sandbox. So the script records the window on the real
desktop. When offstage works again, wrap the `fsvng.exe` line in `offstage run --size 1600x1000`
and record from its `--record` mp4 instead of gdigrab.
