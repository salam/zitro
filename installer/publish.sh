#!/bin/bash
# Publish the checked DMG to https://zitro.spot/downloads/. Replaces the file in one step,
# so a download in progress or a failed upload never leaves a broken installer online.
set -euo pipefail

INSTALLER_DIR="$(cd "$(dirname "$0")" && pwd)"
HOST="${ZITRO_DEPLOY_HOST:-s003.cyon.net}"
REMOTE_DIR="public_html/zitro.spot/downloads"
VERSION="${INSTALLER_VERSION:-1.1}"
NAME="Install-Zitro-$VERSION.dmg"
DMG="$INSTALLER_DIR/$NAME"
URL="https://zitro.spot/downloads/$NAME"

[ -f "$DMG" ] || { echo "Error: build it first: $DMG" >&2; exit 1; }
"$INSTALLER_DIR/scripts/check_release.sh" "$DMG"

HASH=$(shasum -a 256 "$DMG" | cut -d' ' -f1)
ssh "$HOST" "mkdir -p '$REMOTE_DIR'"
scp -q "$DMG" "$HOST:$REMOTE_DIR/.$NAME.upload"
ssh "$HOST" "cd '$REMOTE_DIR' && [ \"\$(sha256sum '.$NAME.upload' | cut -d' ' -f1)\" = '$HASH' ] && chmod 644 '.$NAME.upload' && mv '.$NAME.upload' '$NAME' && echo '$HASH  $NAME' > '$NAME.sha256' && chmod 644 '$NAME.sha256'"

# Trust only what comes back from the public address
LIVE=$(curl -fsSL -A 'zitro-publish' "$URL" | shasum -a 256 | cut -d' ' -f1)
[ "$LIVE" = "$HASH" ] || { echo "Error: the live file doesn't match" >&2; exit 1; }
echo "Live: $URL"
echo "SHA-256: $HASH"
