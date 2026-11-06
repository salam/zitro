#!/bin/bash

# Build DMG installer for AI Stick Flasher
# Creates a macOS disk image with the flasher app

set -e

echo "======================================"
echo "  AI Stick Flasher - DMG Builder"
echo "======================================"
echo ""

# Configuration
APP_NAME="Install Firebird AI Stick"
DMG_NAME="Firebird-AI-Stick-Installer"
VERSION="1.0"
INSTALLER_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$INSTALLER_DIR/build"
DMG_TEMP="$BUILD_DIR/${DMG_NAME}-temp.dmg"
DMG_FINAL="$INSTALLER_DIR/${DMG_NAME}-${VERSION}.dmg"
VOLUME_NAME="Firebird AI Stick Installer"
VOLUME_ICON="$INSTALLER_DIR/assets/volume_icon.icns"

# Clean up previous builds
echo "Cleaning previous builds..."
rm -rf "$BUILD_DIR"
rm -f "$DMG_FINAL"
mkdir -p "$BUILD_DIR"

# Create staging directory
STAGING="$BUILD_DIR/dmg"
mkdir -p "$STAGING"

echo "Copying application..."
cp -R "$INSTALLER_DIR/$APP_NAME.app" "$STAGING/"

echo "Copying README..."
cp "$INSTALLER_DIR/README.txt" "$STAGING/"

# Calculate size needed for DMG
echo "Calculating DMG size..."
SIZE=$(du -sm "$STAGING" | cut -f1)
SIZE=$((SIZE + 20))  # Add 20MB padding

echo "Creating temporary DMG (${SIZE}MB)..."
hdiutil create \
    -volname "$VOLUME_NAME" \
    -fs HFS+ \
    -fsargs "-c c=64,a=16,e=16" \
    -format UDRW \
    -size ${SIZE}m \
    -srcfolder "$STAGING" \
    "$DMG_TEMP"

echo "Mounting temporary DMG..."
MOUNT_DIR=$(hdiutil attach "$DMG_TEMP" | grep Volumes | sed 's/.*\/Volumes/\/Volumes/')

if [ -z "$MOUNT_DIR" ]; then
    echo "Error: Failed to mount DMG"
    exit 1
fi

echo "Mounted at: $MOUNT_DIR"

# Copy files to DMG
echo "Copying files to DMG..."
cp -R "$STAGING/"* "$MOUNT_DIR/"

# Create background image
BACKGROUND_DIR="$MOUNT_DIR/.background"
mkdir -p "$BACKGROUND_DIR"

BACKGROUND_IMG="$INSTALLER_DIR/assets/background.png"
if [ -f "$BACKGROUND_IMG" ]; then
    echo "Adding background image..."
    cp "$BACKGROUND_IMG" "$BACKGROUND_DIR/background.png"
fi

# Hide README
echo "Configuring window appearance..."
SetFile -a V "$MOUNT_DIR/README.txt" 2>/dev/null || true

# Set custom icon if available
if [ -f "$VOLUME_ICON" ]; then
    echo "Setting custom volume icon..."
    cp "$VOLUME_ICON" "$MOUNT_DIR/.VolumeIcon.icns"
    SetFile -a C "$MOUNT_DIR"
fi

# Configure DMG window appearance with AppleScript
echo "Setting DMG window layout..."
sleep 2  # Give system time to register the mount

osascript <<APPLESCRIPT
tell application "Finder"
    tell disk "$VOLUME_NAME"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {400, 100, 1000, 600}
        set viewOptions to the icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 128

        -- Set background (use POSIX path)
        set background picture of viewOptions to POSIX file "${MOUNT_DIR}/.background/background.png"

        -- Position icons
        set position of item "$APP_NAME.app" of container window to {300, 200}
        try
            set position of item "README.txt" of container window to {800, 400}
        end try

        close
        open
        update without registering applications
        delay 2
    end tell
end tell
APPLESCRIPT

# Set permissions
echo "Setting permissions..."
chmod -Rf go-w "$MOUNT_DIR" 2>/dev/null || true

# Unmount
echo "Ejecting temporary DMG..."
hdiutil detach "$MOUNT_DIR" -quiet -force

# Compress and make final DMG
echo "Creating final compressed DMG..."
hdiutil convert "$DMG_TEMP" \
    -format UDZO \
    -imagekey zlib-level=9 \
    -o "$DMG_FINAL"

# Clean up
echo "Cleaning up..."
rm -rf "$BUILD_DIR"

# Make DMG readable
chmod 644 "$DMG_FINAL"

echo ""
echo "======================================"
echo "✓ DMG created successfully!"
echo "======================================"
echo ""
echo "Output: $DMG_FINAL"
echo "Size: $(du -h "$DMG_FINAL" | cut -f1)"
echo ""
echo "You can now distribute this DMG file."
echo "Users can double-click to mount it and drag the app to Applications."
echo ""
