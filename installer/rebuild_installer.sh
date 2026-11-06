#!/bin/bash
# Build the release from the sketch as it is now: firmware, tests, app, signed and notarized DMG.
#
#   ./rebuild_installer.sh                 Full release (needs notary credentials, see below)
#   ./rebuild_installer.sh --no-notarize   Local test build; macOS warns on other Macs
#
# Notary credentials, either:  NOTARY_PROFILE=<keychain profile>
#                         or:  NOTARY_KEY=<AuthKey.p8> NOTARY_KEY_ID=<id> NOTARY_ISSUER=<uuid>
set -euo pipefail

# Wolfram answers need the AppID, which isn't in git
if ! grep -q 'WOLFRAM_APPID "[A-Z0-9-]\{6,\}"' "$(dirname "$0")/../sketch/sketch_jan12a/local_secrets.h" 2>/dev/null; then
  echo "Error: sketch/sketch_jan12a/local_secrets.h with the Wolfram AppID is missing (see local_secrets.h.example)" >&2
  exit 1
fi

INSTALLER_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$INSTALLER_DIR")"
SKETCH="$PROJECT_ROOT/sketch/sketch_jan12a/sketch_jan12a.ino"
FQBN="esp32:esp32:m5stack_stickc_plus2"
export INSTALLER_VERSION="1.1"

BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT

echo "== 1/6 Compiling the firmware from scratch"
# Always a new build folder. Copying from an old one is how January's firmware,
# with an API key in it, kept shipping in the February installers.
arduino-cli compile --fqbn "$FQBN" \
    --build-property "compiler.c.elf.extra_flags=-Wl,--allow-multiple-definition" \
    --build-path "$BUILD/build" --output-dir "$BUILD/out" "$SKETCH" | tail -2

COMMIT=$(git -C "$PROJECT_ROOT" rev-parse --short HEAD)
DIRTY=""
git -C "$PROJECT_ROOT" diff --quiet HEAD -- sketch/sketch_jan12a ':!sketch/sketch_jan12a/build' || DIRTY="+local"
[ -z "$(git -C "$PROJECT_ROOT" ls-files --others --exclude-standard sketch/sketch_jan12a)" ] || DIRTY="+local"
LABEL="$(date +%Y-%m-%d) ($COMMIT$DIRTY)"

echo "== 2/6 Packaging and checking the firmware"
python3 "$INSTALLER_DIR/scripts/package_firmware.py" "$BUILD/out" "$INSTALLER_DIR/firmware" "$LABEL"
python3 "$INSTALLER_DIR/scripts/scan_secrets.py" "$INSTALLER_DIR/firmware"

echo "== 3/6 Building the flasher"
"$INSTALLER_DIR/native_flasher/build.sh"

echo "== 4/6 Testing the flasher against a simulated ESP32"
python3 "$INSTALLER_DIR/native_flasher/tests/run_tests.py"

echo "== 5/6 Building the app"
"$INSTALLER_DIR/gui_installer/build.sh"

echo "== 6/6 Signing, notarizing and checking the DMG"
"$INSTALLER_DIR/build_dmg.sh" "$@"
