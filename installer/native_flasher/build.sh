#!/bin/bash
# Build the native ESP32 flasher as a universal binary.
set -euo pipefail

FLASHER_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="$FLASHER_DIR/bin"
SDK=$(xcrun --show-sdk-path --sdk macosx)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$BIN_DIR"
for arch in arm64 x86_64; do
    echo "Compiling esp32flash for $arch..."
    swiftc -O -target "$arch-apple-macos11.0" -sdk "$SDK" \
        "$FLASHER_DIR"/Sources/*.swift -o "$WORK/esp32flash-$arch"
done
lipo -create "$WORK/esp32flash-arm64" "$WORK/esp32flash-x86_64" -output "$BIN_DIR/esp32flash"
chmod 755 "$BIN_DIR/esp32flash"
echo "Built $BIN_DIR/esp32flash ($(du -h "$BIN_DIR/esp32flash" | cut -f1))"
