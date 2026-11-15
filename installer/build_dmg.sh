#!/bin/bash
# Sign the app, build the DMG, notarize it, and check the result.
# Stops at the first problem and leaves any earlier DMG in place until the new one is good.
set -euo pipefail

INSTALLER_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_NAME="Install Zitro"
VERSION="${INSTALLER_VERSION:-1.1}"
VOLUME_NAME="Install Zitro"
SOURCE_APP="$INSTALLER_DIR/gui_installer/build/$APP_NAME.app"
FINAL="$INSTALLER_DIR/Install-Zitro-$VERSION.dmg"

NOTARIZE=1
[ "${1:-}" = "--no-notarize" ] && NOTARIZE=0

SIGN_ID="${SIGN_ID:-$(security find-identity -v -p codesigning | sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p' | head -1)}"
[ -n "$SIGN_ID" ] || { echo "Error: no Developer ID Application certificate in the keychain" >&2; exit 1; }

NOTARY_ARGS=()
if [ "$NOTARIZE" = 1 ]; then
    if [ -n "${NOTARY_PROFILE:-}" ]; then
        NOTARY_ARGS=(--keychain-profile "$NOTARY_PROFILE")
    elif [ -n "${NOTARY_KEY:-}" ] && [ -n "${NOTARY_KEY_ID:-}" ] && [ -n "${NOTARY_ISSUER:-}" ]; then
        NOTARY_ARGS=(--key "$NOTARY_KEY" --key-id "$NOTARY_KEY_ID" --issuer "$NOTARY_ISSUER")
    else
        echo "Error: no notary credentials. Set NOTARY_PROFILE, or NOTARY_KEY, NOTARY_KEY_ID and NOTARY_ISSUER." >&2
        echo "       For a local test build use --no-notarize." >&2
        exit 1
    fi
fi

WORK=$(mktemp -d)
MOUNT="$WORK/mount"
cleanup() {
    hdiutil detach "$MOUNT" -quiet -force 2>/dev/null || true
    rm -rf "$WORK"
}
trap cleanup EXIT

APP="$WORK/stage/$APP_NAME.app"
mkdir -p "$WORK/stage"
ditto "$SOURCE_APP" "$APP"
cp "$INSTALLER_DIR/README.txt" "$WORK/stage/"

echo "Signing with $SIGN_ID"
codesign --force --options runtime --timestamp --sign "$SIGN_ID" "$APP/Contents/MacOS/esp32flash"
codesign --force --options runtime --timestamp --sign "$SIGN_ID" "$APP"
codesign --verify --strict --deep "$APP"

echo "Creating the disk image"
hdiutil create -quiet -volname "$VOLUME_NAME" -fs HFS+ -format UDRW -srcfolder "$WORK/stage" "$WORK/rw.dmg"
mkdir -p "$MOUNT"
hdiutil attach -quiet -nobrowse -noautoopen -mountpoint "$MOUNT" "$WORK/rw.dmg"

mkdir -p "$MOUNT/.background"
cp "$INSTALLER_DIR/assets/background.png" "$MOUNT/.background/background.png"
cp "$INSTALLER_DIR/assets/volume_icon.icns" "$MOUNT/.VolumeIcon.icns"
SetFile -a C "$MOUNT" 2>/dev/null || true

# The window layout is a nicety and needs Finder automation: a refusal or a hang must not stop a release
hdiutil detach -quiet "$MOUNT"
hdiutil attach -quiet -noautoopen "$WORK/rw.dmg" -mountpoint "/Volumes/$VOLUME_NAME"
perl -e 'alarm 40; exec @ARGV' osascript <<APPLESCRIPT 2>/dev/null || echo "note: window layout skipped (Finder automation not available)"
tell application "Finder"
    tell disk "$VOLUME_NAME"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {300, 200, 940, 600}
        set viewOptions to the icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 128
        set background picture of viewOptions to file ".background:background.png"
        set position of item "$APP_NAME.app" of container window to {320, 210}
        set position of item "README.txt" of container window to {540, 330}
        update without registering applications
        delay 1
        close
    end tell
end tell
APPLESCRIPT
sync
hdiutil detach -quiet "/Volumes/$VOLUME_NAME" -force

hdiutil convert -quiet "$WORK/rw.dmg" -format UDZO -imagekey zlib-level=9 -o "$WORK/final.dmg"
codesign --force --timestamp --sign "$SIGN_ID" "$WORK/final.dmg"

if [ "$NOTARIZE" = 1 ]; then
    echo "Notarizing (this uploads the DMG to Apple)"
    xcrun notarytool submit "$WORK/final.dmg" "${NOTARY_ARGS[@]}" --wait --timeout 20m | tee "$WORK/notary.log"
    grep -q "status: Accepted" "$WORK/notary.log" || { echo "Error: Apple did not accept the DMG" >&2; exit 1; }
    xcrun stapler staple -q "$WORK/final.dmg"
fi

# Only a checked image may replace the current one
CHECK_ARGS=()
[ "$NOTARIZE" = 1 ] || CHECK_ARGS=(--not-notarized)
"$INSTALLER_DIR/scripts/check_release.sh" "$WORK/final.dmg" ${CHECK_ARGS[@]+"${CHECK_ARGS[@]}"}
mv "$WORK/final.dmg" "$FINAL"
chmod 644 "$FINAL"

echo ""
echo "Ready: $FINAL ($(du -h "$FINAL" | cut -f1))"
shasum -a 256 "$FINAL"
