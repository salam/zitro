#!/bin/bash
# Checks the background WiFi search: join order, fallbacks, retries and the icon's pulse (no hardware needed).
set -e
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/zitro_wifi_test_$$"
c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined test_wifi.cpp -o "$OUT"
"$OUT"
rm -f "$OUT"
