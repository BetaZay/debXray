#!/usr/bin/env bash
set -euo pipefail

APP=debXray
BUILD=build

echo "[*] configuring (Release, fully static)…"
cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release

echo "[*] compiling…"
cmake --build "$BUILD" --target $APP --parallel "$(nproc)"

echo "[*] verifying the binary really is self-contained…"
if ldd "$BUILD/bin/$APP" 2>&1 | grep -q "not a dynamic executable"; then
    echo "[✓] $APP is fully static."
else
    echo "[!] Found dynamic dependencies:"
    ldd "$BUILD/bin/$APP"
    exit 1
fi
