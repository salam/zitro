#!/bin/bash
# Checks what every button does in the tour, and that every tile fits the stick's real fonts (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_guide_test_$$"
M5GFX_FONTS="${M5GFX_FONTS:-$HOME/Documents/Arduino/libraries/M5GFX/src/lgfx/Fonts/Custom}"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined -I"$M5GFX_FONTS" test_guide.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"
