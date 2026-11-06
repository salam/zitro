#!/bin/bash
# Builds and runs the Wolfram result host tests (no hardware, no network).
# Fixtures are real LLM API answers. With an argument, also writes a picture of every page to that .bmp file.
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_wolfram_test"
# The small font's real glyphs, if the library is here. Without it the tests use a stand-in width.
M5GFX="$HOME/Documents/Arduino/libraries/M5GFX/src"
c++ -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -isystem "$M5GFX" \
  test_wolfram.cpp -o "$OUT"
"$OUT" fixtures "$@"
