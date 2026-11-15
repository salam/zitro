#!/usr/bin/env python3
"""Builds the table behind zitro.spot/api/locate.php: roughly where a stick is, from its IPv4 address.

Input: DB-IP's free "IP to City Lite" CSV (https://db-ip.com/db/download/ip-to-city-lite, CC BY 4.0,
monthly), and GeoNames cities1000 plus admin1 codes, as for make_place_data.py. Every DB-IP city becomes
the GeoNames place that names it within 30 km, or else the nearest one in the same country within 100 km:
the stick then gets the same kind of answer as from place.php, with a time zone rule from zones.tsv.

Output, in <out dir> (uploaded to webpage2/api/ipdata/, not committed: 20 MB, new every month):
  ip4.bin     8-byte records, sorted: first address (uint32, big-endian), place number (uint32, 0xFFFFFFFF
              for none). A record holds until the next one starts. Neighbouring ranges of one place merge.
  places.bin  96-byte records: name (64 bytes UTF-8, NUL-padded), region (8: the US state code, else the
              country), country (2), zone number in zones.tsv (uint16 big-endian), latitude and longitude
              (float32 big-endian), 16 bytes spare
  SOURCE.txt  where it comes from and the credit DB-IP asks for

Usage: python3 -I scripts/make_ip_data.py <dbip-city-lite.csv.gz> <cities1000.txt> <admin1CodesASCII.txt>
           <zones.tsv> <out dir>
zones.tsv is the one in webpage2/api/places/, so both lookups number their zones alike.
"""
import csv
import gzip
import ipaddress
import math
import os
import struct
import sys
import unicodedata

NAME_KM = 30       # A GeoNames place with DB-IP's city name this close is that city
NEAREST_KM = 100   # Without a name match: the nearest place, if it's this close
CELL_DEG = 1.0     # Grid for the nearest search
PLACE_SIZE = 96
NONE = 0xFFFFFFFF


def fold(s):
    s = unicodedata.normalize("NFKD", s)
    return "".join(ch for ch in s.lower() if "a" <= ch <= "z")


def km(lat1, lon1, lat2, lon2):
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp, dl = p2 - p1, math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 6371.0 * 2 * math.asin(min(1.0, math.sqrt(a)))


def load_places(cities_path, zone_number):
    places, grid = [], {}
    with open(cities_path, encoding="utf-8", newline="\n") as f:
        for line in f:
            c = line.rstrip("\n").split("\t")
            name, lat, lon, country, admin1, population, zone = (
                c[1], float(c[4]), float(c[5]), c[8], c[10], int(c[14] or 0), c[17])
            if zone not in zone_number or not country:
                continue  # A zone place.php doesn't know: never sent
            index = len(places)
            places.append((name, country, admin1, lat, lon, population, zone_number[zone], fold(name)))
            grid.setdefault((country, math.floor(lat / CELL_DEG), math.floor(lon / CELL_DEG)), []).append(index)
    return places, grid


def best_place(places, grid, country, lat, lon, city):
    """The GeoNames place for a DB-IP city: the biggest of that name within NAME_KM, else the nearest."""
    key = fold(city)
    named, nearest, nearest_km = None, None, NEAREST_KM
    reach = 2  # cells around: 1 degree of longitude is under 112 km
    cy, cx = math.floor(lat / CELL_DEG), math.floor(lon / CELL_DEG)
    for dy in range(-reach, reach + 1):
        for dx in range(-reach, reach + 1):
            for i in grid.get((country, cy + dy, cx + dx), ()):
                p = places[i]
                d = km(lat, lon, p[3], p[4])
                if key and p[7] == key and d <= NAME_KM and (named is None or p[5] > places[named][5]):
                    named = i
                if d < nearest_km:
                    nearest, nearest_km = i, d
    return named if named is not None else nearest


def place_record(p):
    name, country, admin1, lat, lon, _population, zone, _key = p
    region = admin1 if country == "US" and admin1 else country
    raw = name.encode("utf-8")
    if len(raw) > 63:
        raw = raw[:63]
        while raw and (raw[-1] & 0xC0) == 0x80:
            raw = raw[:-1]
        raw = raw[:-1] if raw and raw[-1] >= 0xC0 else raw
    rec = struct.pack(">64s8s2sHff", raw, region.encode("ascii")[:7], country.encode("ascii"), zone, lat, lon)
    return rec + b"\0" * (PLACE_SIZE - len(rec))


def add_range(ranges, first, number, last):
    """Appends [first, number, last], or extends the last range if it's the same place and adjoins."""
    if ranges and ranges[-1][1] == number and ranges[-1][2] + 1 == first:
        ranges[-1][2] = last
    else:
        ranges.append([first, number, last])


def main():
    if len(sys.argv) != 6:
        raise SystemExit(__doc__)
    dbip_path, cities_path, admin_path, zones_path, out_dir = sys.argv[1:]
    del admin_path  # The region is the admin1 code itself; the names aren't needed (kept for symmetry)
    zone_number = {}
    with open(zones_path, encoding="utf-8") as f:
        for i, line in enumerate(f):
            zone_number[line.split("\t")[0]] = i
    places, grid = load_places(cities_path, zone_number)

    chosen, used = {}, {}
    ranges = []
    rows = matched = 0
    with gzip.open(dbip_path, "rt", encoding="utf-8", newline="") as f:
        for start, end, _continent, country, _state, city, lat, lon in csv.reader(f):
            if ":" in start:
                continue  # IPv6: the stick asks over IPv4
            rows += 1
            key = (country, city, lat, lon)
            if key not in chosen:
                chosen[key] = best_place(places, grid, country, float(lat), float(lon), city) if country != "ZZ" else None
            p = chosen[key]
            number = NONE
            if p is not None:
                matched += 1
                number = used.setdefault(p, len(used))
            first, last = int(ipaddress.IPv4Address(start)), int(ipaddress.IPv4Address(end))
            if ranges and ranges[-1][2] + 1 != first:
                add_range(ranges, ranges[-1][2] + 1, NONE, first - 1)  # A gap: no place
            add_range(ranges, first, number, last)

    if ranges and ranges[-1][2] < 0xFFFFFFFF:
        add_range(ranges, ranges[-1][2] + 1, NONE, 0xFFFFFFFF)  # Past the last range: no place
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "ip4.bin"), "wb") as f:
        for first, number, _last in ranges:
            f.write(struct.pack(">II", first, number))
    order = sorted(used, key=lambda p: used[p])
    with open(os.path.join(out_dir, "places.bin"), "wb") as f:
        for p in order:
            f.write(place_record(places[p]))
    with open(os.path.join(out_dir, "SOURCE.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(f"IP Geolocation by DB-IP (https://db-ip.com), CC BY 4.0: {os.path.basename(dbip_path)}\n")
        f.write("Places: GeoNames (https://www.geonames.org), CC BY 4.0, cities1000\n")
        f.write(f"{rows} IPv4 ranges, {matched} with a place, {len(ranges)} records, {len(order)} places\n")
    print(f"{rows} IPv4 ranges, {matched} with a place, {len(ranges)} records, {len(order)} places")


if __name__ == "__main__":
    main()
