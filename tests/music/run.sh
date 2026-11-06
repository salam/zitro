#!/bin/bash
# Builds and runs the music band host tests (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_music_test"
c++ -std=c++17 -O1 -Wall -Wextra test_music.cpp -o "$OUT"
"$OUT"
