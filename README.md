# fsvng

### Your filesystem, but make it *three-dimensional*

**fsvng** is a from-scratch C++17 rewrite of [fsv](http://fsv.sourceforge.net/), the legendary 1999 3D file system visualizer. It turns your directory listings into navigable 3D landscapes.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3%20Core-green?style=flat-square)
![Platforms](https://img.shields.io/badge/platforms-Win%20%7C%20Mac%20%7C%20Linux-lightgrey?style=flat-square)
![Tests](https://img.shields.io/badge/tests-26%20passing-brightgreen?style=flat-square)

---

## What does it do?

Point fsvng at any directory and watch it build a 3D visualization of your filesystem. Navigate through your files the way nature intended: by flying a camera through a city of colored blocks.

**Three visualization modes:**

| Mode | What it looks like | Best for |
|------|-------------------|----------|
| **MapV** | Nested treemap of 3D boxes | Seeing where your disk space went |
| **TreeV** | Radial platforms spiraling upward | Understanding directory hierarchy |
| **DiscV** | Circular discs branching outward | Visualizing file distribution |

**Color modes:** By file type, by modification timestamp, or by wildcard pattern matching.

---

## Quick Start

```bash
# Clone
git clone https://github.com/sp00nznet/fsvng.git
cd fsvng

# Build (dependencies auto-fetched via CMake FetchContent)
cmake -B build -DFSVNG_BUILD_TESTS=ON
cmake --build build --config Release

# Run
./build/src/Release/fsvng          # Windows
./build/src/fsvng                  # Linux/macOS

# Or point it at a specific directory
./build/src/Release/fsvng C:\Users
```

### Prerequisites

- **CMake** 3.20+
- **C++17 compiler** (MSVC 2019+, GCC 9+, Clang 10+)
- **OpenGL 3.3** capable GPU
- That's it. Everything else is fetched automatically.

---

## Controls

| Input | Action |
|-------|--------|
| **Left drag** | Orbit camera |
| **Right drag** | Pan camera |
| **Scroll wheel** | Zoom in/out |
| **W/S or Up/Down** | Dolly forward/back |
| **A/D or Left/Right** | Strafe left/right |
| **Q/E** | Move up/down |
| **Double-click** | Navigate into directory / open file |
| **Enter** | Expand/collapse current directory |
| **Backspace** | Navigate back |
| **Click tree panel** | Navigate to node |
| **Toolbar: Bird's Eye** | Toggle overhead view |

---

## Tech Stack

All dependencies are fetched at build time. Zero manual installs.

| Component | Library |
|-----------|---------|
| Windowing | SDL2 |
| GUI | Dear ImGui (docking branch) |
| 3D Rendering | OpenGL 3.3 Core + custom shaders |
| GL Loading | glad |
| Math | GLM |
| Config | nlohmann/json |
| Testing | Google Test |

---

## Project Structure

```
fsvng/
  cmake/              CMake dependency fetching
  shaders/             GLSL vertex/fragment shaders
  src/
    app/               Application lifecycle + config
    core/              FsNode, FsTree, scanner, types
    animation/         Morph engine, scheduler, frame loop
    camera/            Mode-specific cameras + view matrices
    renderer/          Shaders, meshes, text, picking
    geometry/          MapV/TreeV/DiscV layout algorithms
    color/             Color assignment (type/timestamp/wildcard)
    ui/                ImGui panels, toolbar, menus, dialogs
  tests/               Google Test suites (26 tests)
  docs/                Architecture & porting notes
```

---

## Running Tests

```bash
cd build
ctest --build-config Release --output-on-failure
```

All 26 tests across 7 test executables should pass.

---

## Lineage

The original **fsv** was written in 1999 by [Daniel Richard G.](mailto:skunk@mit.edu) using GTK+ 1.2 and fixed-function OpenGL. It was brilliant but hasn't compiled on anything modern since roughly the Bush administration.

**fsvng** is a complete rewrite: ~82 new C++17 source files replacing ~25,000 lines of C. The visualization algorithms (treemap packing, radial tree layout, disc arrangement) are faithfully ported. Everything else is modern.

See [docs/PORTING.md](docs/PORTING.md) for the full archaeology report.

---

## License

GPL-2.0, same as the original fsv. See [COPYING](COPYING) for details.
