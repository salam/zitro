#!/bin/bash
# Builds and runs the globe's host tests (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_globe_test_$$"
LIBRARIES="$(arduino-cli config get directories.user 2>/dev/null || echo "$HOME/Documents/Arduino")/libraries"
c++ -std=c++17 -O1 -Wall -Wextra -I"$LIBRARIES/ArduinoJson/src" test_globe.cpp -o "$OUT"
"$OUT" fixtures ../../webpage2/data/globe/coast_detail.bin
rm -f "$OUT"
