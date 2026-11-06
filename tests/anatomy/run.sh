#!/bin/bash
# Builds and runs the anatomy host tests (no hardware needed), with memory and undefined-behaviour checks on.
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_anatomy_test"
c++ -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined test_anatomy.cpp -o "$OUT"
"$OUT" ../../webpage2/data/anatomy
