#!/bin/bash
# Builds and runs the sky view host tests (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_sky_test"
c++ -std=c++17 -O1 -Wall -Wextra test_sky.cpp -o "$OUT"
"$OUT"
