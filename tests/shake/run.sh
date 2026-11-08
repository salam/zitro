#!/bin/bash
# Checks every surprise a shake can show against the closed lists of the features (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_shake_test_$$"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined test_shake.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"
