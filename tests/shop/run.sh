#!/bin/bash
# Builds and runs the shop game host tests (no hardware needed). "render" also draws the game's screens as PNG.
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_shop_test_$$"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined test_shop.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"
