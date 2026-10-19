# Changelog

All notable changes to fsvng. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
versions: [SemVer](https://semver.org/).

## [Unreleased]

## [0.10.0] - 2026-10-19

First tagged release, with binaries for Windows (x64), Linux (x64, glibc 2.35+) and macOS (Apple
Silicon). They are unsigned: Windows SmartScreen may ask to confirm, and on macOS clear the
quarantine flag first (`xattr -dr com.apple.quarantine .`, see the README).

### Added
- C++17 / SDL2 / Dear ImGui / OpenGL 3.3 rewrite of fsv (1999), with its MapV (treemap) and TreeV
  (radial tree) views.
- Fly-through camera: WASD/arrows to fly and strafe, Q/E for height, right-drag to pan,
  Escape for the parent folder, Backspace for back.
- Labels in MapV and TreeV that skip any that would overlap.
- Right-click menu, including Windows Explorer's own menu for the file on Windows.
- Expand All, in the right-click menu and as `fsvng --expand-all [folder]`.
- Themes (Classic, Tron, Matrix, Synthwave, Ember) and a remembered default folder.
- The view mode, color mode and window size are restored on the next start (the theme already was).
- The version shows in Help > About.
- Background scanning with progress.
- A browser build (Emscripten, WebGL 2).
- CI on Windows, Linux and macOS for every push; pushing a `v*` tag publishes a release.
- `tools/capture-screenshots.sh` regenerates the README screenshots and GIF
  ([docs/screenshots.md](docs/screenshots.md)).

### Changed
- Going up, back or to the root collapses the folder you left at once, instead of animating.
- Shaders load from the executable's folder, so fsvng runs from any working directory.
- `imgui.ini` (panel layout) is saved next to `config.json` instead of the working directory.
- Dear ImGui is pinned to `v1.93.0-docking` instead of following the docking branch.
- The fsv 0.9 sources and autotools build are no longer in the tree (they remain in git history).

### Fixed
- TreeV: expanding a large tree made the whole view vanish. Expand All only marked two levels
  as expanded, deeper folders kept stale widths, and the layout grew its core radius until it
  overflowed. ([docs/treev.md](docs/treev.md))
- TreeV: when the layout grew, the camera kept pointing at where the tree used to be.
- macOS: request the forward-compatible OpenGL 3.3 core context macOS requires.
- The README said GPL-2.0; fsv and fsvng are LGPL.

### Removed
- DiscV view.
- Screenshots that showed a personal folder; the new ones show open-source code.
