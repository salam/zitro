#!/bin/bash
# Checks the home menu: every question's reference opens its view, every label fits the row,
# and the menu's presses, also against M5Unified's button code (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_home_menu_test_$$"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined test_home_menu.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"

# M5Unified's own button code with simulated presses: a menu's clicks never come back as a double or triple press
M5UNIFIED="${M5UNIFIED:-$HOME/Documents/Arduino/libraries/M5Unified/src}"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined -I"$M5UNIFIED" test_button_clicks.cpp \
  "$M5UNIFIED/utility/Button_Class.cpp" -o "$OUT"
"$OUT"
rm -f "$OUT"
