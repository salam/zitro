#!/bin/bash
# Build "Install Zitro.app" (universal, not yet signed) from the firmware package in ../firmware.
set -euo pipefail

GUI_DIR="$(cd "$(dirname "$0")" && pwd)"
INSTALLER_DIR="$(dirname "$GUI_DIR")"
APP_NAME="Install Zitro"
VERSION="${INSTALLER_VERSION:-1.1}"
BUILD_NUMBER="$(date +%Y%m%d%H%M)"
BUILD_DIR="$GUI_DIR/build"
APP="$BUILD_DIR/$APP_NAME.app"
SDK=$(xcrun --show-sdk-path --sdk macosx)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

[ -x "$INSTALLER_DIR/native_flasher/bin/esp32flash" ] || { echo "Error: build esp32flash first" >&2; exit 1; }
[ -f "$INSTALLER_DIR/firmware/manifest.json" ] || { echo "Error: no firmware package in firmware/" >&2; exit 1; }

rm -rf "$BUILD_DIR"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/firmware"

cat > "$WORK/BuildInfo.swift" <<SWIFT
struct BuildInfo {
    static let version = "$VERSION"
    static let buildNumber = "$BUILD_NUMBER"
}
SWIFT

for arch in arm64 x86_64; do
    echo "Compiling $APP_NAME for $arch..."
    swiftc -O -parse-as-library -target "$arch-apple-macos11.0" -sdk "$SDK" \
        "$GUI_DIR"/Sources/*.swift "$WORK/BuildInfo.swift" -o "$WORK/app-$arch"
done
lipo -create "$WORK/app-arm64" "$WORK/app-x86_64" -output "$APP/Contents/MacOS/$APP_NAME"

# The flasher sits next to the app binary, where macOS expects helper executables
cp "$INSTALLER_DIR/native_flasher/bin/esp32flash" "$APP/Contents/MacOS/esp32flash"

# Only the files the manifest names go in: nothing stale can slip into a release
python3 - "$INSTALLER_DIR/firmware/manifest.json" "$APP/Contents/Resources/firmware" <<'PY'
import json, os, shutil, sys
manifest, dest = sys.argv[1], sys.argv[2]
src = os.path.dirname(manifest)
for part in json.load(open(manifest))["parts"]:
    shutil.copy(os.path.join(src, part["file"]), dest)
shutil.copy(manifest, dest)
PY

cp "$INSTALLER_DIR/assets/AppIcon.icns" "$INSTALLER_DIR/assets/plug-in.png" "$INSTALLER_DIR/assets/zitro-mark.png" "$APP/Contents/Resources/"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key><string>$APP_NAME</string>
    <key>CFBundleIdentifier</key><string>spot.zitro.installer</string>
    <key>CFBundleName</key><string>$APP_NAME</string>
    <key>CFBundleDisplayName</key><string>$APP_NAME</string>
    <key>CFBundleIconFile</key><string>AppIcon</string>
    <key>CFBundleShortVersionString</key><string>$VERSION</string>
    <key>CFBundleVersion</key><string>$BUILD_NUMBER</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>LSMinimumSystemVersion</key><string>11.0</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>LSApplicationCategoryType</key><string>public.app-category.utilities</string>
    <key>NSHumanReadableCopyright</key><string>© 2026 Zitro</string>
</dict>
</plist>
PLIST

# Everyone must be able to read a copied app, whatever the source files' modes were
find "$APP" -type d -exec chmod 755 {} +
find "$APP" -type f -exec chmod 644 {} +
chmod 755 "$APP/Contents/MacOS/$APP_NAME" "$APP/Contents/MacOS/esp32flash"
xattr -cr "$APP"

echo "Built $APP"
