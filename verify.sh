#!/bin/bash

# Arduino Sketch Verification Script
# Verifies (compiles) the AI Stick sketch without uploading

set -e  # Exit on error

SKETCH_PATH="sketch/sketch_jan12a/sketch_jan12a.ino"
FQBN="esp32:esp32:m5stack_stickc_plus2"

echo "======================================"
echo "   AI Stick - Sketch Verification"
echo "======================================"
echo ""

# Check if arduino-cli is installed
if ! command -v arduino-cli &> /dev/null; then
    echo "❌ arduino-cli not found!"
    echo ""
    echo "Install with:"
    echo "  brew install arduino-cli"
    echo ""
    echo "Or download from: https://arduino.github.io/arduino-cli/"
    exit 1
fi

echo "✓ arduino-cli found"
echo ""

# Check if ESP32 core is installed
if ! arduino-cli core list | grep -q "esp32:esp32"; then
    echo "⚠️  ESP32 core not installed"
    echo "Installing ESP32 core..."
    arduino-cli core update-index
    arduino-cli core install esp32:esp32
    echo "✓ ESP32 core installed"
    echo ""
fi

# Verify the sketch
echo "Compiling sketch..."
echo "Board: $FQBN"
echo "Sketch: $SKETCH_PATH"
echo ""

if arduino-cli compile --fqbn "$FQBN" --build-property "compiler.c.elf.extra_flags=-Wl,--allow-multiple-definition" "$SKETCH_PATH"; then
    echo ""
    echo "======================================"
    echo "✅ Compilation successful!"
    echo "======================================"
    exit 0
else
    echo ""
    echo "======================================"
    echo "❌ Compilation failed!"
    echo "======================================"
    exit 1
fi
