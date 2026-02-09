# Building fsvng

## Prerequisites

- **CMake** 3.20 or later
- **C++17 compiler**:
  - Windows: Visual Studio 2019+ or MSVC Build Tools
  - Linux: GCC 9+ or Clang 10+
  - macOS: Xcode 12+ or Clang 10+
- **OpenGL 3.3** capable GPU and drivers
- **Internet connection** (first build only, to fetch dependencies)

All library dependencies are fetched automatically via CMake FetchContent:
- SDL2
- Dear ImGui (docking branch)
- glad (OpenGL loader)
- GLM (math)
- nlohmann/json (config persistence)
- Google Test (if tests enabled)

## Windows (Visual Studio)

```powershell
# Configure (generates .sln)
cmake -B build -G "Visual Studio 17 2022" -DFSVNG_BUILD_TESTS=ON

# Build
cmake --build build --config Release

# Run
.\build\src\Release\fsvng.exe
```

Or open `build/fsvng.sln` in Visual Studio and build from the IDE.

## Windows (Ninja / command line)

```powershell
# From a Visual Studio Developer Command Prompt
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DFSVNG_BUILD_TESTS=ON
cmake --build build

.\build\src\fsvng.exe
```

## Linux (Ubuntu/Debian)

```bash
# Install build tools (if needed)
sudo apt install cmake g++ libgl-dev

# Configure & build
cmake -B build -DFSVNG_BUILD_TESTS=ON
cmake --build build -j$(nproc)

# Run
./build/src/fsvng
```

## macOS

```bash
# Install build tools (if needed)
brew install cmake

# Configure & build
cmake -B build -DFSVNG_BUILD_TESTS=ON
cmake --build build -j$(sysctl -n hw.ncpu)

# Run
./build/src/fsvng
```

## Running Tests

```bash
cd build
ctest --build-config Release --output-on-failure
```

Expected output: 7 test executables, 26 tests, all passing.

| Test | What it covers |
|------|---------------|
| test_FsNode | Node creation, tree ops, path resolution |
| test_FsScanner | Scan temp directory, verify counts |
| test_Morph | Easing functions, chaining, break/finish |
| test_MapVLayout | Layout known tree, verify no overlaps |
| test_TreeVLayout | Platform radiuses, arc widths |
| test_ColorSystem | All color modes, spectrum interpolation |
| test_Camera | View matrix computation |

## Troubleshooting

**"gladLoadGL failed"** - Your GPU or drivers don't support OpenGL 3.3. Update your graphics drivers.

**Build fails fetching dependencies** - Check your internet connection. CMake FetchContent downloads SDL2, ImGui, etc. on first build.

**"pwsh.exe not found" warning** - Harmless. The shader copy post-build step tries PowerShell first, falls back to cmd. Shaders are still copied correctly.

**Tests fail with file permission errors** - test_FsScanner creates temp files. Ensure your temp directory is writable.
