#!/usr/bin/env bash
# Install build tooling and SDL2 runtime/dev dependencies for building fsvng on Linux.
# Run with: ./install-deps.sh   (it will prompt for your sudo password)
set -euo pipefail

PACKAGES=(
    cmake
    ninja-build
    build-essential
    libgl-dev
    libx11-dev
    libxext-dev
    libxrandr-dev
    libxcursor-dev
    libxi-dev
    libxfixes-dev
    libxss-dev
    libxkbcommon-dev
    libwayland-dev
)

echo "==> Updating package lists..."
sudo apt-get update

echo "==> Installing: ${PACKAGES[*]}"
sudo apt-get install -y "${PACKAGES[@]}"

echo
echo "==> Done. Verifying key tools:"
cmake --version | head -1
ninja --version
echo "All set. You can now build fsvng."
