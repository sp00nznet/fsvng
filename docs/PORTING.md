# Porting Notes: fsv (1999) -> fsvng (2025)

## What Was fsv?

fsv was a 3D file system visualizer written in 1999 by Daniel Richard G. at MIT. It was ~25,000 lines of C using:

- **GTK+ 1.2** (dead since 2004)
- **GtkGLArea** (legacy OpenGL widget)
- **Fixed-function OpenGL** (glBegin/glEnd, display lists, GL_SELECT picking)
- **GLib 1.x** data structures (GNode, GList, GHashTable)
- **POSIX-only** APIs (scandir, lstat, getpwuid, fnmatch)
- **Autotools** build system

None of this compiles on any modern system.

## What Changed

### Build System
| fsv | fsvng |
|-----|-------|
| autotools (configure.in, Makefile.am) | CMake 3.20+ with FetchContent |
| Manual dependency installation | All deps auto-fetched at build time |
| Linux-only | Windows, macOS, Linux |

### Language & Libraries
| fsv | fsvng |
|-----|-------|
| C (K&R-ish) | C++17 |
| GTK+ 1.2 | Dear ImGui (docking branch) |
| GtkGLArea | SDL2 + OpenGL 3.3 Core |
| GLib (GNode, GList) | STL (vector, unique_ptr, string) |
| Fixed-function GL | Shader-based rendering (GLSL 330) |
| glGenLists/glCallList | VAO/VBO/EBO (MeshBuffer) |
| GL_SELECT picking | Color-based GPU picking (FBO) |
| glPushMatrix/glPopMatrix | Explicit MatrixStack with GLM |
| getpwuid/getgrgid | Platform-abstracted (PlatformUtils) |
| scandir/lstat | std::filesystem |
| fnmatch | Custom wildcard matcher |

### Data Structures
| fsv | fsvng |
|-----|-------|
| `GNode` tree + `NodeDesc` struct | `FsNode` with `vector<unique_ptr<FsNode>>` children |
| `DirNodeDesc` separate struct | Fields merged into `FsNode` |
| `geomparams[5]` void* array + casts | Typed structs: `DiscVGeomParams`, `MapVGeomParams`, `TreeVGeomParams` |
| Global variables everywhere | Singleton classes |
| `GHashTable` for node lookup | `std::unordered_map` in FsTree |

### Visualization Algorithms

The three visualization algorithms were **faithfully ported**:

#### MapV (Treemap) - `geometry.c:474-1202` -> `MapVLayout.cpp`
- Treemap packing algorithm preserved exactly
- Box mesh generation: slanted sides with per-type slant ratios
- Side face normals calculated identically
- `DIR_HEIGHT=384`, `LEAF_HEIGHT=128`, `BORDER_PROPORTION=0.01` constants preserved

#### TreeV (Radial Tree) - `geometry.c:1205-end` -> `TreeVLayout.cpp`
- Maple-derived cubic reshaping formula preserved
- Platform arrangement on concentric rings
- Leaf node layout on platform surfaces
- `MIN_CORE_RADIUS=8192`, `LEAF_NODE_EDGE=256` constants preserved

#### DiscV (Circular) - `geometry.c:73-452` -> `DiscVLayout.cpp`
- Disc arrangement algorithm preserved
- Stagger logic for tight-fit layouts
- Folder outline ring + tab indicator (completing the original WIP)

### Camera System - `camera.c` -> `Camera.cpp`
- All camera modes ported (DiscV top-down, MapV orbital, TreeV cylindrical)
- Morph-driven pans with swing-back for long MapV travels
- Two-stage L-shaped pan for TreeV
- Bird's-eye view toggle
- Added: right-click drag panning (not in original)

### Animation Engine - `animation.c` -> `Morph.cpp` + `Animation.cpp` + `Scheduler.cpp`
- All 5 easing functions preserved: linear, quadratic, inv_quadratic, sigmoid, sigmoid_accel
- Chained morphs, step/end callbacks
- Frame-based event scheduler

### Color System - `color.c` -> `ColorSystem.cpp` + `Spectrum.cpp`
- By-type: static color assignment per NodeType
- By-timestamp: rainbow spectrum mapped to file modification times
- By-wildcard: pattern matching with configurable rules
- 1024-shade spectrum interpolation preserved

## What's New (Not in Original)

1. **Cross-platform support** - Windows + macOS + Linux
2. **Background scanning** - UI stays responsive during filesystem scan
3. **Right-click panning** - Pan the camera target in all modes
4. **Screen-space text labels** - File/folder names overlaid on MapV view
5. **Default path** - Configurable startup directory with JSON persistence
6. **Docking UI** - ImGui docking allows rearranging panels
7. **Modern shader pipeline** - Per-fragment lighting instead of fixed-function

## Source File Mapping

| Original fsv | fsvng equivalent |
|-------------|-----------------|
| `fsv.c` (main) | `main.cpp` + `app/App.cpp` |
| `common.h` (types) | `core/Types.h` + `core/FsNode.h` |
| `common.c` (utils) | `core/PlatformUtils.cpp` |
| `scanfs.c` | `core/FsScanner.cpp` |
| `animation.c` | `animation/Morph.cpp` + `Animation.cpp` + `Scheduler.cpp` |
| `camera.c` | `camera/Camera.cpp` |
| `ogl.c` | `renderer/Renderer.cpp` + `ViewportPanel.cpp` |
| `geometry.c` | `geometry/MapVLayout.cpp` + `TreeVLayout.cpp` + `DiscVLayout.cpp` + `GeometryManager.cpp` |
| `colexp.c` | `geometry/CollapseExpand.cpp` |
| `color.c` | `color/ColorSystem.cpp` + `Spectrum.cpp` |
| `tmaptext.c` | `renderer/TextRenderer.cpp` |
| `about.c` + `fsv3d.h` | `renderer/SplashRenderer.cpp` |
| `window.c` | `ui/MainWindow.cpp` + `MenuBar.cpp` + `Toolbar.cpp` + `StatusBar.cpp` |
| `dirtree.c` | `ui/DirTreePanel.cpp` |
| `filelist.c` | `ui/FileListPanel.cpp` |
| `viewport.c` | `ui/ViewportPanel.cpp` |
| `dialog.c` | `ui/Dialogs.cpp` |
| `callbacks.c` | Distributed into respective UI classes |
| `gui.c` | `ui/ImGuiBackend.cpp` |
| `lib/nvstore.c` | `app/Config.cpp` |

## Lessons Learned

1. **ImGui docking branch quirks**: `BeginViewportSideBar` isn't in the public API. Use manual `SetNextWindowPos/Size` + `Begin` instead.
2. **ImTextureID casting**: Use C-style cast `(ImTextureID)(uintptr_t)(texId)` - `reinterpret_cast` from integer types requires an intermediate `uintptr_t`.
3. **Windows std::filesystem**: `directory_iterator::operator++` can throw on permission errors. Use `dirIt.increment(ec)` with error codes.
4. **Shader uniform names**: Must match exactly between C++ and GLSL. Our shaders use `uModel`/`uView`/`uProjection` (u prefix convention).
5. **Deployment state**: Layout `initRecursive` checks `DirTreePanel::isEntryExpanded()` to set deployment. If the tree panel hasn't been drawn yet, everything starts collapsed. Fix: explicitly expand root before layout init.
6. **Backface culling**: MapV box geometry has mixed face winding. Disable `GL_CULL_FACE` globally.
