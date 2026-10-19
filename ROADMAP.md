# Roadmap

## Next

- **Hand-test the Linux and macOS release builds.** CI builds them and runs the tests, but nobody
  has run either one yet.
- **DPI awareness on Windows.** fsvng doesn't declare itself DPI-aware, so on a scaled display
  Windows stretches it and text is soft. SDL2 can opt in (`SDL_HINT_WINDOWS_DPI_AWARENESS`), but
  then ImGui needs scaling fonts and sizes to match, or everything is tiny.
- **TreeV framing for big trees.** Looking at a folder frames that folder's own platform (as fsv
  did). After expanding a large tree, the root view lands low against the inner ring with the
  subfolders off toward the top. Framing the subtree would be better. See
  [docs/treev.md](docs/treev.md#framing-a-folder-is-close-up-by-design).
- **Double-click in TreeV.** Viewport double-click hit-tests MapV only.
- **Open on macOS and Linux.** The right-click Open item is Windows-only (`ShellExecute`);
  `open` / `xdg-open` would cover the rest.
- **A netlab recipe**, so day-to-day builds and test runs go there instead of the workstation.

## Later

- **Universal (arm64 + x86_64) macOS build.** Release builds are Apple Silicon only.
- **Signed binaries.** Windows SmartScreen and macOS Gatekeeper both warn on first run.
- **3D text labels.** `TextRenderer` is a stub; labels are drawn as a 2D ImGui overlay.
- **Better default wildcard colors.** The by-pattern defaults cover archives, images and audio
  only; a source tree shows almost no color.

## Out of scope

- **DiscV.** Ported, then removed (7adc401).
- **Editing the filesystem.** fsvng shows files; it doesn't move, rename or delete them.
