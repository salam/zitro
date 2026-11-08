#!/bin/bash
# Host tests for the live ISS view: the track the zitro.spot server computes (needs php on the command
# line) and the stick's side of it (no hardware needed)
set -e
cd "$(dirname "$0")"
php test_iss.php
OUT="${TMPDIR:-/tmp}/zitro_iss_test_$$"
LIBRARIES="$(arduino-cli config get directories.user 2>/dev/null || echo "$HOME/Documents/Arduino")/libraries"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined -I"$LIBRARIES/ArduinoJson/src" test_iss_core.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"
