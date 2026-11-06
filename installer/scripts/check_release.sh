#!/bin/bash
# Last gate before a DMG is released: open it the way a user would and check what's really inside.
#   check_release.sh <dmg> [--not-notarized]
set -euo pipefail

DMG="$1"
NOTARIZED=1
[ "${2:-}" = "--not-notarized" ] && NOTARIZED=0
SCRIPTS="$(cd "$(dirname "$0")" && pwd)"
FIRMWARE="$(dirname "$SCRIPTS")/firmware"

MOUNT=$(mktemp -d)
trap 'hdiutil detach "$MOUNT" -quiet -force 2>/dev/null || true; rmdir "$MOUNT" 2>/dev/null || true' EXIT
hdiutil verify -quiet "$DMG"
hdiutil attach -quiet -readonly -nobrowse -noautoopen -mountpoint "$MOUNT" "$DMG"
APP="$MOUNT/Install Zitro.app"
[ -d "$APP" ] || { echo "Error: no app in the DMG" >&2; exit 1; }

echo "- Scanning every file for credentials"
python3 "$SCRIPTS/scan_secrets.py" "$MOUNT"

echo "- Comparing the firmware inside with the package built just now"
diff -r -x '.DS_Store' "$APP/Contents/Resources/firmware" "$FIRMWARE"

echo "- Checking the firmware package with the shipped flasher"
"$APP/Contents/MacOS/esp32flash" --help >/dev/null
python3 - "$APP/Contents/Resources/firmware" <<'PY'
import hashlib, json, os, sys
d = sys.argv[1]
m = json.load(open(os.path.join(d, "manifest.json")))
for p in m["parts"]:
    if hashlib.sha256(open(os.path.join(d, p["file"]), "rb").read()).hexdigest() != p["sha256"]:
        sys.exit(f"Error: {p['file']} doesn't match its manifest")
print(f"  firmware {m['firmware']}, {len(m['parts'])} parts")
PY

echo "- Both binaries run on Intel and Apple Silicon"
for binary in "$APP/Contents/MacOS/Install Zitro" "$APP/Contents/MacOS/esp32flash"; do
    lipo "$binary" -verify_arch arm64 x86_64
done

echo "- Signatures"
codesign --verify --strict --deep "$APP"
codesign --verify --strict "$DMG"
for target in "$APP" "$APP/Contents/MacOS/esp32flash"; do
    details=$(codesign -dv "$target" 2>&1)
    grep -q 'flags=.*runtime' <<<"$details" || { echo "Error: no hardened runtime on $target" >&2; exit 1; }
    grep -q 'Timestamp=' <<<"$details" || { echo "Error: no secure timestamp on $target" >&2; exit 1; }
    grep -q 'TeamIdentifier=[A-Z0-9]' <<<"$details" || { echo "Error: not a Developer ID signature on $target" >&2; exit 1; }
done

if [ "$NOTARIZED" = 1 ]; then
    echo "- Notarization and Gatekeeper"
    xcrun stapler validate -q "$DMG"
    spctl -a -t open --context context:primary-signature "$DMG"
    spctl -a -t exec "$APP"
else
    echo "- NOT notarized: macOS will warn on other Macs. Do not publish this build."
fi
echo "Release check passed"
