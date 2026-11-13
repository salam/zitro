#!/bin/bash
# Builds and runs the vocabulary trainer host tests (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_vocab_test_$$"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined test_vocab.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"
