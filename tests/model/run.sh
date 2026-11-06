#!/bin/bash
# Builds and runs the 3D view host tests (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_model_test"
c++ -std=c++17 -O1 -Wall -Wextra test_model.cpp -o "$OUT"
"$OUT" fixtures
