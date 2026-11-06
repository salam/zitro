#!/bin/bash
# Builds and runs the recipe host tests (no hardware needed). ArduinoJson comes from the Arduino library folder,
# so these tests run the same JSON parser as the stick.
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_recipe_test"
JSON_SRC="${ARDUINOJSON_SRC:-$HOME/Documents/Arduino/libraries/ArduinoJson/src}"
c++ -std=c++17 -O1 -Wall -Wextra -I"$JSON_SRC" test_recipe.cpp -o "$OUT"
"$OUT" fixtures ../../webpage2/data/recipe
if [ -f ../../scripts/make_recipe_data.py ]; then
  python3 -I -B test_generator.py
fi
