#!/bin/bash
# Host tests for speech-to-text at Infomaniak (stt_core.h), no hardware needed
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_stt_test_$$"
LIBRARIES="$(arduino-cli config get directories.user 2>/dev/null || echo "$HOME/Documents/Arduino")/libraries"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined -I"$LIBRARIES/ArduinoJson/src" test_stt_core.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"
