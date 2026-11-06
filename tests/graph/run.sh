#!/bin/bash
# Builds and runs the graph host tests (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_graph_test"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined test_graph.cpp -o "$OUT"
"$OUT"
