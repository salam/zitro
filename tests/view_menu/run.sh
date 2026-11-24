#!/bin/bash
# Checks the menu a view can bring: its rows and its presses (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_view_menu_test_$$"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined test_view_menu.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"
