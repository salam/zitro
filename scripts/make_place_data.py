#!/usr/bin/env python3
"""Builds the place table behind zitro.spot/api/place.php: webpage2/api/places/

The stick's own table (clock_data.h) holds 320 big cities. For every other place the stick asks the
server, which answers from these files.

Places: GeoNames cities1000 (CC BY 4.0), https://download.geonames.org/export/dump/ with
admin1CodesASCII.txt and countryInfo.txt. Time zones: GeoNames names the IANA zone of every place; the
zone's rule comes from the IANA tz database as installed, the same way as for clock_data.h.

Written:
  xx.tsv.gz    the places whose key starts with xx, biggest first for each key:
               key, name, country, admin1, latitude, longitude, zone number
  zones.tsv    zone number's line: IANA name, POSIX rule
  regions.tsv  folded region or country name, then what it means: US or US.TN (for "Paris, Texas")
  SOURCE.txt   what this was built from

Keys fold exactly as the firmware's clockFold does, because the stick sends the folded name.

Usage: python3 -I scripts/make_place_data.py <cities1000.txt> <admin1CodesASCII.txt> <countryInfo.txt>
           <zoneinfo dir> <download date> <out dir>
"""
import gzip
import hashlib
import importlib.util
import io
import os
import sys

_spec = importlib.util.spec_from_file_location(
    "make_clock_data", os.path.join(os.path.dirname(os.path.abspath(__file__)), "make_clock_data.py"))
_clock = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_clock)
fold, fold_german, screen_name, posix_rule = _clock.fold, _clock.fold_german, _clock.screen_name, _clock.posix_rule

MAX_KEY = 48
ALTERNATES_FROM_POPULATION = 100_000  # Other names (Cologne for Köln) only for places this big
DISTRICT_WEIGHT = 0.1                 # A part of a city only beats a town if it's far bigger (Brooklyn does)
ALTERNATE_WEIGHT = 0.1                # Likewise another name: Cologne is Köln, not the village in Italy
COUNTRY_ALIASES = {"US": ["USA", "United States of America", "America"], "GB": ["UK", "Great Britain", "England"]}


def main():
    if len(sys.argv) != 7:
        raise SystemExit(__doc__)
    cities_path, admin_path, country_path, zoneinfo, date, out_dir = sys.argv[1:]

    zones, zone_number = [], {}
    rows = []
    place_count = 0
    with open(cities_path, encoding="utf-8", newline="\n") as f:
        for line in f:
            c = line.rstrip("\n").split("\t")
            if len(c) < 19:
                raise SystemExit(f"short line: {line[:60]}")
            name, ascii_name, alternates, lat, lon, code, country, admin1, population, zone = (
                c[1], c[2], c[3], float(c[4]), float(c[5]), c[7], c[8], c[10], int(c[14] or 0), c[17])
            if not zone:
                raise SystemExit(f"{name} ({country}): no time zone")
            if zone not in zone_number:
                zone_number[zone] = len(zones)
                zones.append((zone, posix_rule(zoneinfo, zone)))  # Stops if the zone isn't in the tz database
            place_count += 1
            weight = population * (DISTRICT_WEIGHT if code == "PPLX" else 1)
            shown = screen_name(name)
            own = {fold(name), fold(ascii_name)}
            other = {fold_german(name)}
            if population >= ALTERNATES_FROM_POPULATION:
                for a in alternates.split(","):
                    if a and all(ord(ch) < 0x180 for ch in a) and len(fold(a)) >= 4:
                        other.add(fold(a))
            record = (shown, country, admin1, f"{lat:.3f}", f"{lon:.3f}", str(zone_number[zone]))
            for keys, factor in ((own, 1), (other - own, ALTERNATE_WEIGHT)):
                for key in keys:
                    if 2 <= len(key) <= MAX_KEY:
                        rows.append((key, -weight * factor, record))
    rows.sort(key=lambda r: (r[0], r[1], r[2]))

    os.makedirs(out_dir, exist_ok=True)
    shards = {}
    for key, _weight, record in rows:
        shards.setdefault(key[:2], io.StringIO()).write("\t".join((key,) + record) + "\n")
    for prefix, text in shards.items():
        # mtime 0: the same input gives the same bytes
        with open(os.path.join(out_dir, prefix + ".tsv.gz"), "wb") as raw:
            with gzip.GzipFile(filename="", mode="wb", compresslevel=9, fileobj=raw, mtime=0) as gz:
                gz.write(text.getvalue().encode("utf-8"))

    with open(os.path.join(out_dir, "zones.tsv"), "w", encoding="utf-8", newline="\n") as f:
        for zone, rule in zones:
            f.write(f"{zone}\t{rule}\n")

    regions = set()
    with open(country_path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            c = line.rstrip("\n").split("\t")
            for n in [c[0], c[4]] + COUNTRY_ALIASES.get(c[0], []):
                regions.add((fold(n), c[0]))
    with open(admin_path, encoding="utf-8") as f:
        for line in f:
            c = line.rstrip("\n").split("\t")
            country, _, code = c[0].partition(".")
            regions.add((fold(c[2]), c[0]))
            regions.add((fold(c[1]), c[0]))
            if code.isalpha():
                regions.add((fold(code), c[0]))
    with open(os.path.join(out_dir, "regions.tsv"), "w", encoding="utf-8", newline="\n") as f:
        for key, scope in sorted(r for r in regions if r[0]):
            f.write(f"{key}\t{scope}\n")

    with open(cities_path, "rb") as f:
        digest = hashlib.sha256(f.read()).hexdigest()
    version_path = os.path.join(zoneinfo, "+VERSION")
    tz_version = open(version_path).read().strip() if os.path.exists(version_path) else "unknown"
    with open(os.path.join(out_dir, "SOURCE.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("Generated by scripts/make_place_data.py. Do not edit by hand.\n")
        f.write(f"Places: GeoNames cities1000 (CC BY 4.0, https://www.geonames.org/), downloaded {date}, sha256 {digest}\n")
        f.write(f"Time zones: IANA tz database {tz_version}\n")
        f.write(f"{place_count} places, {len(rows)} names, {len(zones)} zones\n")
    print(f"{place_count} places, {len(rows)} names, {len(zones)} zones, {len(shards)} files -> {out_dir}")


if __name__ == "__main__":
    main()
