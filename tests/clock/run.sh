#!/bin/bash
# Builds and runs the world clock's host tests (no hardware needed). The time zone check compares every
# zone with this computer's own tz database, so it needs one (macOS and Linux have it).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_clock_test_$$"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined test_clock.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"
