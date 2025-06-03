#!/usr/bin/env bash
set -euo pipefail

APP=debXray
BUILD=build

echo "[*] Configuring (Release)…"
cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release

echo "[*] Building..."
cmake --build "$BUILD" --target "$APP" --parallel "$(nproc)"

echo "[*] Packaging into .deb..."
cd "$BUILD"
cpack -G DEB

echo "[*] Installing package..."
sudo dpkg -i debXray-1.0.0-Linux.deb