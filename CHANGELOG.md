# Changelog

All notable changes to fsvng. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
versions: [SemVer](https://semver.org/).

## [Unreleased]

## [0.10.0] - 2026-10-18

First tagged release. Prebuilt binaries for Windows (x64), Linux (x64) and macOS (Apple Silicon).
The binaries are unsigned: on macOS, right-click → Open the first time; on Windows, SmartScreen
may ask you to confirm.

### Added
- C++17 / SDL2 / Dear ImGui / OpenGL 3.3 rewrite of fsv (1999).
- MapV (treemap) and TreeV (radial tree) visualization modes.
- Fly-through camera: WASD/arrow dolly and strafe, Q/E vertical, right-drag pan, navigate-up shortcut.
- Double-click to navigate into directories or launch files; auto-expand on navigate.
- On-screen labels for MapV and TreeV.
- Right-click context menu, themes, and a remembered default scan path.
- Background directory scanning with progress.
- CI on Windows, Linux and macOS; tagged pushes publish release binaries.

### Changed
- Shaders load from the executable's directory, so fsvng runs from any working directory.

### Fixed
- macOS: request a forward-compatible OpenGL 3.3 core context (required there).

### Removed
- DiscV mode.
