#!/bin/bash

# Quick Rebuild Script for AI Stick Flasher Installer
# This script compiles the firmware and rebuilds the DMG installer

set -e

echo "======================================"
echo "  AI Stick Installer - Quick Rebuild"
echo "======================================"
echo ""

# Get the project root (parent of installer directory)
INSTALLER_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$INSTALLER_DIR")"

echo "Project root: $PROJECT_ROOT"
echo "Installer directory: $INSTALLER_DIR"
echo ""

# Step 1: Compile firmware
echo "Step 1/6: Compiling firmware..."
echo "--------------------------------"
cd "$PROJECT_ROOT"
./verify.sh

if [ ! -f ".build/sketch_jan12a.ino.merged.bin" ]; then
    echo "Error: Firmware binary not found after compilation!"
    exit 1
fi

echo ""
echo "✓ Firmware compiled successfully"
echo ""

# Step 2: Build native flasher (universal binary)
echo "Step 2/6: Building native flasher (universal)..."
echo "--------------------------------------------------"
cd "$INSTALLER_DIR"
./native_flasher/build.sh
echo ""

# Step 3: Update firmware in installer
echo "Step 3/6: Updating firmware in installer..."
echo "--------------------------------------------"

# Find boot_app0.bin from Arduino SDK
BOOT_APP0=$(find ~/Library/Arduino15/packages/esp32/hardware/esp32 -name "boot_app0.bin" | head -1)
if [ -z "$BOOT_APP0" ]; then
    echo "Warning: boot_app0.bin not found in Arduino SDK, using existing copy"
fi

# Update individual firmware parts
cp .build/sketch_jan12a.ino.bootloader.bin "$INSTALLER_DIR/firmware/bootloader.bin"
echo "✓ Updated bootloader.bin"
cp .build/sketch_jan12a.ino.partitions.bin "$INSTALLER_DIR/firmware/partitions.bin"
echo "✓ Updated partitions.bin"
if [ -n "$BOOT_APP0" ]; then
    cp "$BOOT_APP0" "$INSTALLER_DIR/firmware/boot_app0.bin"
    echo "✓ Updated boot_app0.bin"
fi
cp .build/sketch_jan12a.ino.bin "$INSTALLER_DIR/firmware/application.bin"
echo "✓ Updated application.bin"

# Update firmware in app bundle
APP_FW_DIR="$INSTALLER_DIR/Install Firebird AI Stick.app/Contents/Resources/firmware"
mkdir -p "$APP_FW_DIR"
cp "$INSTALLER_DIR/firmware/bootloader.bin" "$APP_FW_DIR/"
cp "$INSTALLER_DIR/firmware/partitions.bin" "$APP_FW_DIR/"
cp "$INSTALLER_DIR/firmware/boot_app0.bin" "$APP_FW_DIR/"
cp "$INSTALLER_DIR/firmware/application.bin" "$APP_FW_DIR/"
echo "✓ Updated app bundle firmware"

# Show firmware sizes
echo "  Firmware parts:"
for f in bootloader.bin partitions.bin boot_app0.bin application.bin; do
    SIZE=$(ls -lh "$INSTALLER_DIR/firmware/$f" | awk '{print $5}')
    echo "    $f: $SIZE"
done
echo ""

# Step 4: Build GUI installer (universal binary)
echo "Step 4/6: Building GUI installer (universal)..."
echo "-------------------------------------------------"
cd "$INSTALLER_DIR"
./gui_installer/build.sh

# Copy rebuilt app bundle to installer root
echo "Copying rebuilt app bundle..."
rm -rf "$INSTALLER_DIR/Install Firebird AI Stick.app"
cp -R "$INSTALLER_DIR/gui_installer/build/Install Firebird AI Stick.app" "$INSTALLER_DIR/"
echo "✓ App bundle updated"
echo ""

# Step 5: Rebuild DMG
echo "Step 5/6: Rebuilding DMG installer..."
echo "-------------------------------------"
cd "$INSTALLER_DIR"
./build_dmg.sh

echo ""

# Step 6: Verify DMG
echo "Step 6/6: Verifying DMG..."
echo "--------------------------"
DMG_FILE="$INSTALLER_DIR/Firebird-AI-Stick-Installer-1.0.dmg"

if [ ! -f "$DMG_FILE" ]; then
    echo "Error: DMG file not found!"
    exit 1
fi

hdiutil verify "$DMG_FILE" > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✓ DMG verification successful"
else
    echo "✗ DMG verification failed!"
    exit 1
fi

DMG_SIZE=$(du -h "$DMG_FILE" | cut -f1)
echo "  DMG size: $DMG_SIZE"

echo ""
echo "======================================"
echo "✓ Installer rebuilt successfully!"
echo "======================================"
echo ""
echo "Output: $DMG_FILE"
echo ""
echo "The installer is ready for distribution."
echo "Users can double-click the DMG to install."
echo ""
