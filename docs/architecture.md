# Architecture

## Overview

fsvng uses a singleton-based architecture where each major subsystem is a single-instance class. The main loop runs in `App::run()`, which drives the ImGui frame loop and dispatches to subsystems.

```
App::run()
  -> processEvents()       SDL2 event pump
  -> Animation::tick()     Morph engine + scheduled events
  -> MainWindow::draw()    ImGui UI (dispatches to all panels)
     -> MenuBar / Toolbar / DirTreePanel / FileListPanel / ViewportPanel / StatusBar
     -> ViewportPanel renders 3D scene to FBO via GeometryManager + Camera
```

## Module Map

### Core (`src/core/`)
- **Types.h** - Fundamental types: RGBcolor, XYvec, XYZvec, RTvec, RTZvec, NodeType, FsvMode
- **FsNode** - Unified filesystem node. Owns children via `vector<unique_ptr<FsNode>>`. Carries per-mode geometry params (MapV/TreeV) directly on the node.
- **FsTree** - Singleton tree container with lookup by ID/path
- **FsScanner** - Background-thread filesystem scanner using `std::filesystem`
- **PlatformUtils** - Cross-platform user/group names, size formatting

### Animation (`src/animation/`)
- **Morph** - Tween engine with 5 easing functions (linear, quadratic, inv_quadratic, sigmoid, sigmoid_accel). Supports chained morphs and step/end callbacks. `shift()` moves a value together with any morph running on it.
- **Animation** - Frame loop driver, redraw requests, framerate tracking
- **Scheduler** - Delayed event queue (fire callback after N frames)

### Camera (`src/camera/`)
- **Camera** - Mode-specific camera states stored in a union. Supports revolve, dolly, pan, lookAt (with morph animation), and bird's-eye view toggle.
  - MapV: XYZ target + spherical coordinates (theta/phi/distance)
  - TreeV: RTZ cylindrical target + spherical offset. The target radius is absolute, so it has to
    move when the TreeV core radius does: see [treev.md](treev.md)

### Renderer (`src/renderer/`)
- **ShaderProgram** - GLSL compile/link wrapper
- **MeshBuffer** - VAO/VBO/EBO management. Vertex format: position[3], normal[3], color[3], texcoord[2]
- **Renderer** - Top-level renderer singleton, shader management
- **TextRenderer** - Texture-mapped 3D text (stub, labels done via ImGui overlay)
- **NodePicker** - Color-based GPU picking via offscreen FBO
- **SplashRenderer** - 3D "fsv" logo animation

### Geometry (`src/geometry/`)
- **GeometryManager** - Mode dispatch + helper calculations (MapV z-stacking, TreeV absolute radius/angle)
- **MapVLayout** - Treemap packing algorithm. Builds slanted-box meshes.
- **TreeVLayout** - Radial tree with Maple-derived cubic reshaping. Platforms arranged on concentric rings. Traps: [treev.md](treev.md)
- **CollapseExpand** - Directory expand/collapse animation via deployment morphs. Expand All (`ExpandRecursive`) marks the whole subtree expanded in the tree panel too, because TreeV reads the panel to decide what is a leaf

### Color (`src/color/`)
- **ColorSystem** - Three color assignment modes: by node type, by modification timestamp (rainbow spectrum), by wildcard pattern
- **Spectrum** - 1024-shade rainbow and heat gradients

### UI (`src/ui/`)
- **ImGuiBackend** - SDL2+OpenGL ImGui initialization. Points `imgui.ini` (panel layout) at the config folder
- **MainWindow** - Dockspace layout, navigation state, mode/color management, background scanning
- **DirTreePanel** - Directory tree using `ImGui::TreeNodeEx`
- **FileListPanel** - File list using `ImGui::BeginTable`
- **ViewportPanel** - 3D viewport rendering to FBO, displayed as `ImGui::Image`, with screen-space text label overlay
- **MenuBar** - File/Vis/Colors/Themes/Help menus
- **Toolbar** - Back, CD Root, CD Up, Bird's Eye, mode radio buttons
- **StatusBar** - Status messages
- **Dialogs** - Change Root, Set Default Path, Color Config, About, Properties, the right-click menu (and on Windows the native Explorer menu)

### App (`src/app/`)
- **App** - Command line (`[--expand-all] [path]`), SDL2 window, OpenGL context, main loop. Loads the config before creating the window, applies the saved view mode, color mode, theme and window size, and saves them on exit
- **Config** - JSON config persistence: `%APPDATA%\fsvng\config.json` on Windows, `~/Library/Application Support/fsvng/config.json` on macOS, `$XDG_CONFIG_HOME/fsvng/config.json` or `~/.config/fsvng/config.json` on Linux

## Data Flow

### Filesystem Scan
```
User action (startup / File > Change Root)
  -> MainWindow::requestScan(path)
  -> Background std::thread runs FsScanner::scan()
  -> Progress reported via mutex-protected atomics
  -> MainWindow::finishScan() on completion:
     -> FsTree::setRoot(tree)
     -> ColorSystem::assignRecursive()
     -> MainWindow::initVisualization()
        -> DirTreePanel::setEntryExpanded(rootDir)
        -> GeometryManager::init(mode) -> Layout::init()
        -> Camera::init(mode)
        -> with --expand-all, once: CollapseExpand(rootDir, ExpandRecursive)
```

### Rendering Pipeline
```
ViewportPanel::draw()
  -> Create/resize FBO
  -> Bind FBO, clear, enable depth test
  -> Camera::getViewMatrix() + getProjectionMatrix()
  -> Set shader uniforms (lighting)
  -> GeometryManager::draw() dispatches to active layout
     -> Layout walks FsNode tree recursively
     -> For each node: build vertex data, upload to MeshBuffer, draw
     -> MatrixStack handles model transforms (translate/rotate/scale)
  -> Unbind FBO
  -> ImGui::Image(fbo_texture) displays result
  -> Screen-space text labels projected via viewProj matrix
```

## Key Design Decisions

1. **Geometry params on FsNode**: Each node carries MapV/TreeV geometry structs directly. This avoids external maps and makes tree traversal during layout/draw simple.

2. **FBO-based viewport**: The 3D scene renders to an offscreen framebuffer, then displays as an ImGui image. This integrates cleanly with ImGui's docking system.

3. **Background scanning**: Filesystem scanning runs on a separate thread with mutex-protected progress reporting, keeping the UI responsive.

4. **No backface culling**: MapV box geometry has mixed winding order (the slanted side faces), so face culling is globally disabled during viewport rendering.

5. **Root directory auto-expansion**: The root directory is always set as expanded in DirTreePanel before geometry init. Without this, `initRecursive` sets deployment=0 and the root starts collapsed.
