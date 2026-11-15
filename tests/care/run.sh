#!/bin/bash
# Host tests for the worries rule of the main prompt (care_core.h), no hardware needed
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_care_test_$$"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined test_care.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"
