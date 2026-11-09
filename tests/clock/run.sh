#!/bin/bash
# Builds and runs the world clock's host tests (no hardware needed). The time zone checks compare every
# zone with this computer's own tz database, so they need one (macOS and Linux have it). The place lookup
# needs php and python3. GEONAMES_CITIES names another cities file for the fold comparison.
set -e
cd "$(dirname "$0")"
TEMP="$(mktemp -d "${TMPDIR:-/tmp}/zitro_clock_test_XXXXXX")"
trap 'rm -rf "$TEMP"' EXIT
LIBRARIES="$(arduino-cli config get directories.user 2>/dev/null || echo "$HOME/Documents/Arduino")/libraries"

c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined test_clock.cpp -o "$TEMP/clock"
"$TEMP/clock"

python3 -I ../../scripts/make_place_data.py fixtures/cities.txt fixtures/admin1.txt fixtures/countries.txt /usr/share/zoneinfo test "$TEMP/places" >/dev/null
php test_place.php "$TEMP/places"

python3 -I - "${GEONAMES_CITIES:-fixtures/cities.txt}" "$TEMP/fold.tsv" ../../scripts/make_clock_data.py <<'PY'
import importlib.util, sys
spec = importlib.util.spec_from_file_location("make_clock_data", sys.argv[3])
clock = importlib.util.module_from_spec(spec)
spec.loader.exec_module(clock)
names = {"Zürich", "Łódź", "Straße", "Þingvellir", "Ærø", "Saint-Étienne", "Şanlıurfa", "Đà Nẵng", "東京", "O'Fallon"}
for line in open(sys.argv[1], encoding="utf-8"):
    c = line.split("\t")
    names.update(n for n in [c[1], c[2]] + c[3].split(",") if n)
with open(sys.argv[2], "w", encoding="utf-8", newline="\n") as out:
    for n in sorted(names):
        if "\t" not in n and "\n" not in n:
            out.write(f"{n}\t{clock.fold(n)}\n")
PY

c++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined -I"$LIBRARIES/ArduinoJson/src" test_place_core.cpp -o "$TEMP/place"
"$TEMP/place" ../../webpage2/api/places/zones.tsv "$TEMP/fold.tsv"
