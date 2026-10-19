# Contributing

Issues and pull requests are welcome.

## Before opening a PR

1. Build and run the tests on your platform (see [docs/building.md](docs/building.md)):
   ```
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DFSVNG_BUILD_TESTS=ON
   cmake --build build --config Release --parallel
   ctest --test-dir build --build-config Release
   ```
2. Run fsvng on a real folder in both MapV and TreeV. Several bugs only show up in one view, or
   only after expanding a large tree (try right-click > Expand All on the root).
3. Add a line under `## [Unreleased]` in [CHANGELOG.md](CHANGELOG.md).
4. If the change makes a doc wrong, fix the doc in the same PR.

CI builds and tests every PR on Windows, Linux and macOS.

## Where your code comes from

fsvng is LGPL-2.1-or-later, because it is ported from fsv, which is LGPL. Code you contribute
has to be something you can license that way:

- **Fine:** your own code; code under MIT, BSD, zlib, ISC, Apache-2.0 or similar, with its notice
  kept and a line in [NOTICE](NOTICE); LGPL-2.1-or-later or GPL-compatible code that allows
  "or later" relicensing.
- **Not fine:** GPL-2.0-only or GPL-3.0 code (it would make fsvng GPL), anything non-commercial
  or unlicensed, and anything copied from a project without checking its licence.

If a fix came from reading another project's code, say which project in the PR.

## AI-assisted contributions

Welcome, provided a human has read the change, understood it, and run it.
