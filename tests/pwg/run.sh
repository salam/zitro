#!/bin/bash
# Builds and runs the PWG raster host test (needs libcups, included in macOS).
# Optional argument: path for a printable A4 sample page.
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/pwg_raster_test"
c++ -std=c++17 -O2 -Wall -Wno-deprecated-declarations pwg_raster_test.cpp -lcups -o "$OUT"
"$OUT" "$@"
