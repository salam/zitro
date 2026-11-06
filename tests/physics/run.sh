#!/bin/bash
# Builds and runs the physics and solid host tests (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_physics_test"
mkdir -p "$OUT"
SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined"
for t in test_*.cpp; do
  c++ -std=c++17 -O1 -g -Wall -Wextra $SAN "$t" -o "$OUT/${t%.cpp}"
  "$OUT/${t%.cpp}"
done
