#!/bin/bash
# Builds and runs the timeline host tests (no hardware needed). The generated files are the fixtures.
# Also decodes every prepared picture with the decoder the stick uses, if M5GFX is installed here.
set -e
cd "$(dirname "$0")"
python3 -I test_make_timeline_data.py
OUT="${TMPDIR:-/tmp}/zitro_timeline_test"
c++ -std=c++17 -O1 -Wall -Wextra test_timeline.cpp -o "$OUT"
"$OUT" ../../webpage2/data/timeline

TJPGD="$HOME/Documents/Arduino/libraries/M5GFX/src/lgfx/utility"
if [ -f "$TJPGD/lgfx_tjpgd.c" ]; then
  cc -O2 -c -w -I"$TJPGD" "$TJPGD/lgfx_tjpgd.c" -o "$OUT.tjpgd.o"
  c++ -std=c++17 -O2 -Wall -Wextra -I"$TJPGD" test_decode.cpp "$OUT.tjpgd.o" -o "$OUT.decode"
  "$OUT.decode" ../../webpage2/data/timeline
else
  echo "M5GFX not found: pictures were NOT decoded with the stick's decoder"
fi
