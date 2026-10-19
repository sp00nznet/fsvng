# Building fsvng

CI builds and tests every push on Windows, Linux and macOS
([.github/workflows/ci.yml](../.github/workflows/ci.yml)); the commands below are the ones it runs.

## Prerequisites

- **CMake** 3.20 or later
- **C++17 compiler**: Visual Studio 2022 (CI uses the `windows-latest` image), GCC 9+ or Clang 10+
  on Linux, Xcode's Clang on macOS
- **Python 3 with jinja2**: `pip install jinja2`. glad generates the OpenGL loader at build time
  with it (see [Troubleshooting](#troubleshooting))
- **git** and an internet connection for the first configure

CMake FetchContent downloads the libraries on the first configure
([cmake/FetchDependencies.cmake](../cmake/FetchDependencies.cmake)). Each is pinned to a tag so a
release can be rebuilt exactly:

| Library | Tag | Used for |
|---|---|---|
| SDL2 | `release-2.30.10` | window, input, GL context |
| Dear ImGui | `v1.93.0-docking` | all UI |
| glad | `v2.0.8` | OpenGL 3.3 core loader |
| GLM | `1.0.1` | math |
| nlohmann/json | `v3.11.3` | config file |
| GoogleTest | `v1.14.0` | tests only ([tests/CMakeLists.txt](../tests/CMakeLists.txt)) |

## Windows

```
cmake -B build -DFSVNG_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release
build\src\Release\fsvng.exe C:\some\folder
```

The default generator is Visual Studio, which puts the binary in `build\src\Release\`. With Ninja
from a Developer Command Prompt (`-G Ninja -DCMAKE_BUILD_TYPE=Release`) it is `build\src\fsvng.exe`.

## Linux

SDL2 is built from source, and needs the windowing headers:

```
sudo apt-get install -y cmake g++ python3-jinja2 libgl1-mesa-dev libegl1-mesa-dev libx11-dev \
  libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxfixes-dev \
  libwayland-dev libxkbcommon-dev libdecor-0-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFSVNG_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build
./build/src/fsvng ~/some/folder
```

Release binaries are built on Ubuntu 22.04 so they run on glibc 2.35 and newer.

## macOS

```
brew install cmake
pip3 install jinja2
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFSVNG_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build
./build/src/fsvng ~/some/folder
```

macOS only gives a 3.3 core context to programs that ask for a forward-compatible one;
`App::init` sets that flag under `__APPLE__`.

## Browser (Emscripten)

fsvng also builds to WebGL 2, live at [sp00.nz/#fsvng](https://sp00.nz/#fsvng).

```
emcmake cmake -S . -B ../fsvng-web -G Ninja -DCMAKE_BUILD_TYPE=Release -DFSVNG_WEB_SAMPLE=/path/to/a/folder
cmake --build ../fsvng-web    # -> src/fsvng.js, fsvng.wasm, fsvng.data
```

A page can't read your disk, so the build bundles one folder (`FSVNG_WEB_SAMPLE`, default `src/`)
and opens it at `/fsvng`. Keep the build directory outside that folder. Everything that differs
from the desktop is behind `__EMSCRIPTEN__` and in [cmake/Web.cmake](../cmake/Web.cmake):
Emscripten's SDL2 port, GLES 3 instead of glad, shaders rewritten to GLSL ES 3.00 as they load,
the browser driving the main loop, and the scan running on the main thread (no threads there).

## Tests

`ctest` runs 7 executables, 27 tests:

| Test | What it covers |
|------|---------------|
| test_FsNode | Node creation, tree ops, path resolution |
| test_FsScanner | Scan a temp directory, verify counts |
| test_Morph | Easing, break/finish, `shift` (the TreeV camera relies on it) |
| test_MapVLayout | Layout a known tree, verify no overlaps |
| test_TreeVLayout | Platform radiuses, arc widths |
| test_ColorSystem | All color modes, spectrum interpolation |
| test_Camera | View matrix computation |

## Releases

Push a `vX.Y.Z` tag. CI builds all three platforms, packages each as the executable, `shaders/`,
`LICENSE`, `NOTICE`, `README.md` and `LICENSES/` (the dependencies' licence files, copied from
the fetched sources), and publishes a GitHub Release whose notes are that version's section of
[CHANGELOG.md](../CHANGELOG.md). The release job fails if the changelog has no section for the tag.

## Troubleshooting

**`ModuleNotFoundError: No module named 'jinja2'`, then `error MSB8066: Custom build for
...khrplatform.h.rule ... exited with code 1`.** glad's CMake runs `find_package(Python)`, which
picks the newest Python on the machine, not the one on `PATH`. If that one lacks jinja2, the
loader generation fails. Install jinja2 into it, or point CMake at the right one:

```
cmake -B build -DPython_EXECUTABLE="$(python -c 'import sys; print(sys.executable)')"
```

CI hit exactly this on the Windows and macOS runners, where `setup-python`'s Python wasn't the
newest one installed; the workflow now passes `-DPython_EXECUTABLE` the same way.

**"gladLoadGL failed"**: the GPU or driver doesn't offer OpenGL 3.3. Update the graphics driver.

**Blank or black viewport, shaders failing to load**: the `shaders` folder must sit next to the
executable. fsvng finds it with `SDL_GetBasePath()`, so the working directory doesn't matter.

**Tests fail with file permission errors**: test_FsScanner creates temp files; the temp directory
must be writable.
