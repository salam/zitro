#!/bin/bash
# Fetches the M5Stack libraries the StickS3 build needs and prints the arduino-cli flags that
# select them. M5Unified 0.2.12 (installed for the Plus2) breaks the StickS3: its microphone is
# never switched on (fixed in 0.2.13), ESP32-S3 audio samples come out of order (0.2.20) and
# its clock chip is driven as the wrong part (0.2.27). The Plus2 keeps the installed versions.
#
# Usage: FLAGS=$(scripts/sticks3_libraries.sh) && arduino-cli compile $FLAGS ...
# Pinned by commit, not just tag: a moved tag must not change what gets built.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIR="$ROOT/.build/libraries-sticks3"

LIBS=(
  "M5Unified 0.2.29 44952cb8e4ac88bf04ad23b9dfd6f4b05c788348"
  "M5GFX 0.2.36 7cb26b44ab31bb41a0a597196dd2c6d868869a21"
)

mkdir -p "$DIR"
for entry in "${LIBS[@]}"; do
  read -r name tag commit <<< "$entry"
  dest="$DIR/$name"
  if [ -d "$dest/.git" ] && [ "$(git -C "$dest" rev-parse HEAD)" = "$commit" ]; then
    echo "--library $dest"
    continue
  fi
  rm -rf "$dest"
  git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$tag" "https://github.com/m5stack/$name.git" "$dest" >&2
  actual="$(git -C "$dest" rev-parse HEAD)"
  if [ "$actual" != "$commit" ]; then
    echo "❌ $name $tag is commit $actual, expected $commit" >&2
    rm -rf "$dest"
    exit 1
  fi
  echo "--library $dest"
done
