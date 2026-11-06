#!/bin/bash
# Builds and runs the protein and crystal host tests (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_protein_test"
c++ -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined test_protein.cpp -o "$OUT"
"$OUT" ../../webpage2/data/protein
python3 -I test_make_protein_data.py
