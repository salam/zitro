#!/bin/bash
# Host tests for read-aloud at Azure Speech (tts_core.h), no hardware needed
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_tts_test_$$"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined test_tts_core.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"
