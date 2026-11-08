#!/bin/bash
# Makes sketch/sketch_jan12a/guide_moon.h, the Moon picture of the guided tour. macOS tools only.
# Source: File:Moon nearside LRO.jpg, Wikimedia Commons, public domain, NASA/GSFC/Arizona State University.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=sketch/sketch_jan12a/guide_moon.h
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
URL=$(curl -fsS -A 'ZitroBuild/1.0' 'https://commons.wikimedia.org/w/api.php?action=query&format=json&prop=imageinfo&iiprop=url&titles=File:Moon_nearside_LRO.jpg' \
  | python3 -I -c 'import json,sys; print(next(iter(json.load(sys.stdin)["query"]["pages"].values()))["imageinfo"][0]["url"])')
curl -fsS -A 'ZitroBuild/1.0' -o "$WORK/moon.jpg" "$URL"
sips -Z 96 "$WORK/moon.jpg" --out "$WORK/small.jpg" >/dev/null
sips --padToHeightWidth 135 240 --padColor 000000 -s format jpeg -s formatOptions 70 "$WORK/small.jpg" --out "$WORK/guide_moon.jpg" >/dev/null
sips -g pixelWidth -g pixelHeight "$WORK/guide_moon.jpg" | grep -q 'pixelWidth: 240' || { echo "wrong width"; exit 1; }
sips -g pixelHeight "$WORK/guide_moon.jpg" | grep -q 'pixelHeight: 135' || { echo "wrong height"; exit 1; }
if xxd -p "$WORK/guide_moon.jpg" | tr -d '\n' | grep -q 'ffc2'; then echo "progressive JPEG"; exit 1; fi
SIZE=$(wc -c < "$WORK/guide_moon.jpg" | tr -d ' ')
[ "$SIZE" -lt 16000 ] || { echo "too large: $SIZE"; exit 1; }
{
  echo "// The Moon picture of the guided tour. Made by scripts/make_guide_moon.sh, don't edit."
  echo "// Source: File:Moon nearside LRO.jpg, Wikimedia Commons. Public domain. NASA/GSFC/Arizona State University."
  echo "#pragma once"
  echo "#include <pgmspace.h>"
  echo "static const unsigned int GUIDE_MOON_JPG_LEN = $SIZE;"
  echo "static const uint8_t GUIDE_MOON_JPG[] PROGMEM = {"
  xxd -i < "$WORK/guide_moon.jpg"
  echo "};"
} > "$OUT"
cp "$WORK/guide_moon.jpg" "${TMPDIR:-/tmp}/guide_moon_check.jpg"
echo "guide_moon.h: $SIZE bytes"
