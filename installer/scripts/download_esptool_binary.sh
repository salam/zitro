#!/bin/bash

# Download precompiled esptool binary from Espressif
# This is a standalone binary with no Python dependency

set -e

echo "======================================"
echo "  esptool Binary Downloader"
echo "======================================"
echo ""

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="$SCRIPT_DIR/../bin"
mkdir -p "$BIN_DIR"

# Espressif provides precompiled binaries
# Check: https://github.com/espressif/esptool/releases

# For macOS, we need the Darwin binary
ESPTOOL_VERSION="v4.7.0"
DOWNLOAD_URL="https://github.com/espressif/esptool/releases/download/${ESPTOOL_VERSION}/esptool-${ESPTOOL_VERSION}-macos.tar.gz"

echo "Downloading esptool ${ESPTOOL_VERSION} for macOS..."
echo "URL: $DOWNLOAD_URL"
echo ""

# Download
TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

if ! curl -L -o esptool.tar.gz "$DOWNLOAD_URL"; then
    echo "Error: Download failed"
    echo ""
    echo "Espressif may not provide precompiled binaries for this version."
    echo "Alternative: We'll need to create a native Swift/C++ flasher"
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Extract
tar -xzf esptool.tar.gz

# Find the binary
if [ -f "esptool" ]; then
    cp esptool "$BIN_DIR/"
    chmod +x "$BIN_DIR/esptool"
elif [ -f "*/esptool" ]; then
    cp */esptool "$BIN_DIR/"
    chmod +x "$BIN_DIR/esptool"
else
    echo "Error: Binary not found in archive"
    ls -la
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Clean up
cd -
rm -rf "$TEMP_DIR"

echo ""
echo "======================================"
echo "✓ esptool binary downloaded!"
echo "======================================"
echo ""
echo "Location: $BIN_DIR/esptool"
echo "Size: $(du -h "$BIN_DIR/esptool" | cut -f1)"
echo ""
echo "Testing binary..."
"$BIN_DIR/esptool" version

echo ""
echo "✓ Ready to bundle with installer"
echo ""
