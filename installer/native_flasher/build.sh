#!/bin/bash

# Build Native ESP32 Flasher

set -e

echo "======================================"
echo "  Building Native ESP32 Flasher"
echo "======================================"
echo ""

FLASHER_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="$FLASHER_DIR/bin"

# Create bin directory
mkdir -p "$BIN_DIR"

# Source files
SOURCES=(
    "$FLASHER_DIR/Sources/SerialPort.swift"
    "$FLASHER_DIR/Sources/SLIPProtocol.swift"
    "$FLASHER_DIR/Sources/ESP32Protocol.swift"
    "$FLASHER_DIR/Sources/StubLoader.swift"
    "$FLASHER_DIR/Sources/Flasher.swift"
    "$FLASHER_DIR/Sources/main.swift"
)
SDK=$(xcrun --show-sdk-path --sdk macosx)

# Compile for ARM64 (Apple Silicon)
echo "Compiling for arm64 (Apple Silicon)..."
swiftc -O \
    -target arm64-apple-macos11.0 \
    -sdk "$SDK" \
    -framework Foundation \
    "${SOURCES[@]}" \
    -o "$BIN_DIR/esp32flash-arm64"

# Compile for x86_64 (Intel)
echo "Compiling for x86_64 (Intel)..."
swiftc -O \
    -target x86_64-apple-macos11.0 \
    -sdk "$SDK" \
    -framework Foundation \
    "${SOURCES[@]}" \
    -o "$BIN_DIR/esp32flash-x86_64"

# Create universal binary
echo "Creating universal binary..."
lipo -create \
    "$BIN_DIR/esp32flash-arm64" \
    "$BIN_DIR/esp32flash-x86_64" \
    -output "$BIN_DIR/esp32flash"

# Clean up arch-specific binaries
rm -f "$BIN_DIR/esp32flash-arm64" "$BIN_DIR/esp32flash-x86_64"

if [ ! -f "$BIN_DIR/esp32flash" ]; then
    echo "Error: Compilation failed"
    exit 1
fi

echo "✓ Universal binary created (arm64 + x86_64)"

# Make executable
chmod +x "$BIN_DIR/esp32flash"

# Show size
SIZE=$(ls -lh "$BIN_DIR/esp32flash" | awk '{print $5}')
echo ""
echo "======================================"
echo "✓ Build complete!"
echo "======================================"
echo ""
echo "Binary: $BIN_DIR/esp32flash"
echo "Size: $SIZE"
echo ""
echo "To test: $BIN_DIR/esp32flash --help"
echo ""
