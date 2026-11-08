#!/bin/bash
# Builds and runs the music band host tests (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_music_test"
c++ -std=c++17 -O1 -Wall -Wextra test_music.cpp -o "$OUT"
"$OUT" ../../webpage2/data/music
"$OUT" --dump | python3 -I "$PWD/check_references.py"
# The song tool, and the firmware reading its output
python3 -I "$PWD/test_make_music_data.py" "$OUT"
# The server data must be exactly what the song files build: no stale, missing or hand-edited file
if [ -d ../../webpage2/data/music ]; then
  python3 -I ../../scripts/make_music_data.py --check ../../webpage2/data/music
fi
