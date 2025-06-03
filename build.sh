#!/usr/bin/env bash
set -euo pipefail

APP=debXray
BUILD=build

echo "[*] Configuring (Release, partially static)…"
cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release

echo "[*] Building..."
cmake --build "$BUILD" --target "$APP" --parallel "$(nproc)"

echo "[*] Checking linked dependencies..."
ldd "$BUILD/bin/$APP" || true
echo

# Check for unwanted dynamic libs
echo "[*] Verifying minimal dynamic dependencies..."
BAD_LIBS=$(ldd "$BUILD/bin/$APP" | grep -v -E 'libopencv|linux-vdso|ld-linux|libc.so|libm.so|libpthread.so|librt.so|libdl.so' || true)

if [[ -z "$BAD_LIBS" ]]; then
    echo "[✓] Portable build clean (OpenCV allowed)."
else
    echo "[!] Found unexpected dynamic deps:"
    echo "$BAD_LIBS"
    exit 1
fi
