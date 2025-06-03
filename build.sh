#!/bin/bash
set -e

APP_NAME="debXray"
BUILD_DIR="build"
OUTPUT_DIR="./bin"

echo "[*] Cleaning previous build..."
rm -rf "$BUILD_DIR" "$OUTPUT_DIR"
mkdir -p "$BUILD_DIR" "$OUTPUT_DIR"

echo "[*] Configuring CMake..."
cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Release

echo "[*] Building $APP_NAME..."
cmake --build "$BUILD_DIR" --target "$APP_NAME" -- -j$(nproc)

echo "[*] Stripping binary..."
strip "$BUILD_DIR/$APP_NAME" || echo "[!] strip failed, skipping"

echo "[*] Copying to $OUTPUT_DIR/"
cp "$BUILD_DIR/$APP_NAME" "$OUTPUT_DIR/"

echo "[✓] Done. Binary located at $OUTPUT_DIR/$APP_NAME"