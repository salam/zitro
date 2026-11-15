#!/usr/bin/env python3
"""Generates the place-name grid for the live ISS view: which place lies under each point of the Earth.
Usage: python3 -I scripts/make_iss_places.py <download dir> <output dir>
The download dir holds these untouched files from Natural Earth 5.1.2 (public domain, see docs/features/iss.md):
  https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/<name>.geojson
for each <name> in SOURCES below. The output dir gets places.bin (the grid) and places.json (the names).

webpage2/api/iss.php looks up the place under the ISS every few seconds along its track. The stick shows
the name exactly as the source spells it: no grammar, no guessing. Fails loudly on anything unexpected:
an input that isn't the pinned file, a name the stick's font can't draw, a name too long for its buffer.
"""
import array
import hashlib
import json
import math
import os
import struct
import sys

NE = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/"
SOURCES = {  # File: SHA-256 of the pinned download
    "ne_10m_geography_marine_polys": "53f865e8ffa966cdd402145c82c5cd14ee7ce974cd0eb9a3f59f03a4cfd2d66c",
    "ne_10m_admin_0_countries": "239eec57ac17f100a11e2536cffc56752c318b50ae765b0918ff7aab4ce8f255",
    "ne_10m_admin_1_states_provinces": "22d0e3ad85eb3e27f17cabf8ba2d50e554fbc27a87796ff891d958185da62fb5",
    "ne_10m_geography_regions_polys": "b7b26e50ea917d3696aec87f932def2bf5f890f5770e441d59c162c6f4c92a77",
    "ne_10m_lakes": "2d036f53dedec578001c5c30c2959ee7d4eebc1306900fa4367c49929ec8f2d9",
    "ne_10m_admin_0_disputed_areas": "9cafef8b7dfb6b164dc58f218f981f4ace9f716f6c03795d4c62d1ac9f3d50f5",
}

CELL_DEG = 0.25  # About 28 km at the equator: the ISS crosses a cell in under 4 seconds
COLS, ROWS = int(360 / CELL_DEG), int(180 / CELL_DEG)

# Countries that show their states, provinces or cantons instead of the country's name: big federal
# countries, where the state says more than the country (Alaska, not "Vereinigte Staaten"), and the three
# German-speaking ones, which kids here know by their cantons and states.
STATE_COUNTRIES = {"US", "CA", "AU", "BR", "RU", "CN", "IN", "MX", "AR", "DE", "AT", "CH"}

# An island's name replaces the country's when the island is smaller than this and nearly all of it lies in
# one country: Korsika, Sizilien, Kreta. Bigger islands are often a country's heartland (Honshu, Great
# Britain), and an island shared by countries (Ireland, Borneo) is better told by the country.
ISLAND_MAX_KM2 = 100000
ISLAND_ONE_COUNTRY = 0.9  # Share of the island's cells in its main country
DISPUTED_WHOLE_COUNTRY = 0.9
LAKE_MAX_SCALERANK = 1    # Only big lakes: Great Lakes, Lake Victoria, Lake Titicaca... (rank 2 has "Woods")

# Natural Earth splits the big oceans into north and south but names both halves the same in German
# ("Pazifischer Ozean"). Keyed on the source's English name; the German follows it word for word.
OCEANS = {
    "North Pacific Ocean": ("Nordpazifik", "North Pacific"),
    "South Pacific Ocean": ("Südpazifik", "South Pacific"),
    "North Atlantic Ocean": ("Nordatlantik", "North Atlantic"),
    "South Atlantic Ocean": ("Südatlantik", "South Atlantic"),
}

NAME_BYTES = 47  # The stick keeps 48 bytes per name, with the terminating zero
FONT_LAST = 0x5FF  # The stick's 10 pt font has glyphs from 0x20 to here


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# Typographic quotes and dashes aren't in the stick's font: the plain ones mean the same
TYPOGRAPHY = str.maketrans({"\u2018": "'", "\u2019": "'", "\u201c": '"', "\u201d": '"', "\u2013": "-", "\u2014": "-"})


def tidy(name):
    """The name as the source has it, with all-caps names ("TASMANIEN") in normal case."""
    if not name:
        return ""
    name = " ".join(name.split())
    name = name.translate(TYPOGRAPHY)
    if name.isupper():
        name = " ".join(w[:1] + w[1:].lower() for w in name.split(" "))
    return name


def check_name(name, where):
    if len(name.encode("utf-8")) > NAME_BYTES:
        sys.exit(f"Name too long for the stick ({len(name.encode('utf-8'))} bytes): {name!r} in {where}")
    for ch in name:
        if ord(ch) < 0x20 or ord(ch) > FONT_LAST:
            sys.exit(f"Character U+{ord(ch):04X} the stick can't draw: {name!r} in {where}")


def polygons(geometry):
    if not geometry:
        return []
    if geometry["type"] == "Polygon":
        return [geometry["coordinates"]]
    if geometry["type"] == "MultiPolygon":
        return geometry["coordinates"]
    sys.exit(f"Unexpected geometry {geometry['type']}")


def cells(geometry):
    """Grid cells whose centre lies inside the geometry (even-odd, so holes stay out)."""
    out = []
    for polygon in polygons(geometry):
        crossings = {}
        for ring in polygon:
            n = len(ring)
            for i in range(n):
                x1, y1 = ring[i][0], ring[i][1]
                x2, y2 = ring[(i + 1) % n][0], ring[(i + 1) % n][1]
                if y1 == y2:
                    continue
                lo, hi = min(y1, y2), max(y1, y2)
                # Rows whose centre latitude is in [lo, hi)
                r_first = math.floor((90 - hi) / CELL_DEG - 0.5) + 1
                r_last = math.floor((90 - lo) / CELL_DEG - 0.5)
                for r in range(max(r_first, 0), min(r_last, ROWS - 1) + 1):
                    lat = 90 - (r + 0.5) * CELL_DEG
                    if not (lo <= lat < hi):
                        continue
                    crossings.setdefault(r, []).append(x1 + (lat - y1) * (x2 - x1) / (y2 - y1))
        for r, xs in crossings.items():
            xs.sort()
            for a, b in zip(xs[0::2], xs[1::2]):
                c_first = math.ceil((a + 180) / CELL_DEG - 0.5)
                c_last = math.ceil((b + 180) / CELL_DEG - 0.5) - 1
                for c in range(max(c_first, 0), min(c_last, COLS - 1) + 1):
                    out.append(r * COLS + c)
    return out


def area_km2(geometry):
    """Area on a sphere of radius 6371 km, outer rings minus holes."""
    total = 0.0
    for polygon in polygons(geometry):
        for k, ring in enumerate(polygon):
            s = 0.0
            for i in range(len(ring)):
                x1, y1 = ring[i][0], ring[i][1]
                x2, y2 = ring[(i + 1) % len(ring)][0], ring[(i + 1) % len(ring)][1]
                s += math.radians(x2 - x1) * (2 + math.sin(math.radians(y1)) + math.sin(math.radians(y2)))
            a = abs(s) * 6371.0 ** 2 / 2
            total += a if k == 0 else -a
    return total


class Places:
    def __init__(self):
        self.names = [("", "")]  # Id 0: no place
        self.ids = {}
        self.grid = array.array("H", [0]) * (COLS * ROWS)

    def id(self, de, en, where):
        de, en = tidy(de), tidy(en)
        if not de or not en:
            sys.exit(f"Missing name in {where}: {de!r} / {en!r}")
        check_name(de, where)
        check_name(en, where)
        key = (de, en)
        if key not in self.ids:
            self.ids[key] = len(self.names)
            self.names.append(key)
        return self.ids[key]

    def paint(self, geometry, place_id):
        for i in cells(geometry):
            self.grid[i] = place_id


def load(directory, name):
    with open(os.path.join(directory, name + ".geojson"), encoding="utf-8") as f:
        return json.load(f)["features"]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, out_dir = sys.argv[1], sys.argv[2]
    sources = []
    for name, prefix in SOURCES.items():  # prefix: the full pinned SHA-256
        digest = sha256(os.path.join(src, name + ".geojson"))
        if digest != prefix:
            sys.exit(f"{name}.geojson isn't the pinned Natural Earth 5.1.2 file (SHA-256 {digest})")
        sources.append({"file": name + ".geojson", "url": NE + name + ".geojson", "sha256": digest})

    p = Places()
    stats = {}

    # 1. Seas and oceans. Bigger ones first, so a sea inside an ocean's outline keeps its own name.
    marine = [f for f in load(src, "ne_10m_geography_marine_polys") if f["properties"].get("name")]
    marine.sort(key=lambda f: -area_km2(f["geometry"]))
    for f in marine:
        q = f["properties"]
        de, en = OCEANS.get(q["name"], (q.get("name_de") or q.get("name_en") or q["name"], q.get("name_en") or q["name"]))
        p.paint(f["geometry"], p.id(de, en, "marine " + q["name"]))
    stats["seas"] = len(marine)

    # 2. Countries, as Natural Earth draws them (who governs the area); disputed areas go blank in step 6
    countries = load(src, "ne_10m_admin_0_countries")
    country_of = {}
    for f in countries:
        q = f["properties"]
        i = p.id(q.get("NAME_DE") or q["NAME_EN"], q["NAME_EN"], "country " + q["ADMIN"])
        country_of[i] = q["ADMIN"]
        p.paint(f["geometry"], i)
    stats["countries"] = len(countries)
    country_grid = array.array("H", p.grid)  # Which country each cell is in, for the island rule

    # 3. States, provinces and cantons of STATE_COUNTRIES
    # Two placeholders without any name (MEX+99?, RUS+99?) are left out: the country shows there.
    states = [f for f in load(src, "ne_10m_admin_1_states_provinces")
              if f["properties"].get("iso_a2") in STATE_COUNTRIES and f["properties"].get("name")]
    for f in states:
        q = f["properties"]
        en = q.get("name_en") or q["name"]
        p.paint(f["geometry"], p.id(q.get("name_de") or en, en, "state " + q["name"]))
    stats["states"] = len(states)

    # 4. Islands below ISLAND_MAX_KM2 that lie nearly all in one country. Bigger ones first, so an island
    # inside an island group keeps its own name.
    islands = []
    for f in load(src, "ne_10m_geography_regions_polys"):
        q = f["properties"]
        if q.get("FEATURECLA") not in ("Island", "Island group"):
            continue
        area = area_km2(f["geometry"])
        if area >= ISLAND_MAX_KM2:
            continue
        inside = [country_grid[i] for i in cells(f["geometry"])]
        land = [c for c in inside if c in country_of]
        if not land:
            continue  # Smaller than a cell, or only over water in the grid
        main_share = max(land.count(c) for c in set(land)) / len(land)
        if main_share < ISLAND_ONE_COUNTRY:
            continue
        islands.append((area, f))
    islands.sort(key=lambda x: -x[0])
    for _, f in islands:
        q = f["properties"]
        en = q.get("NAME_EN") or q["NAME"]
        p.paint(f["geometry"], p.id(q.get("NAME_DE") or en, en, "island " + q["NAME"]))
    stats["islands"] = len(islands)

    # 5. Big lakes, over the land around them
    lakes = [f for f in load(src, "ne_10m_lakes")
             if f["properties"].get("name") and f["properties"].get("scalerank") is not None
             and f["properties"]["scalerank"] <= LAKE_MAX_SCALERANK]
    for f in lakes:
        q = f["properties"]
        en = q["name"]  # name_en is cut short ("Chad", "Titicaca"); name is the full one ("Lake Chad")
        p.paint(f["geometry"], p.id(q.get("name_de") or en, en, "lake " + q["name"]))
    stats["lakes"] = len(lakes)

    # 6. Disputed regions inside a country get no name at all, whoever governs them: Krim, Kashmir, Donezk,
    # Abchasien... (the user's choice, 2026-11-16). Natural Earth's layer also lists whole countries and
    # territories that someone claims (Israel, Taiwan, Kosovo, Falklandinseln): an area covering at least
    # DISPUTED_WHOLE_COUNTRY of a country as Natural Earth draws it keeps that country's name.
    country_cells = {}
    for c in country_grid:
        country_cells[c] = country_cells.get(c, 0) + 1
    disputed = load(src, "ne_10m_admin_0_disputed_areas")
    blanked, kept = [], []
    for f in disputed:
        q = f["properties"]
        name = tidy((q.get("NAME_DE") or q.get("BRK_NAME") or "").lstrip("\ufeff"))
        area = cells(f["geometry"])
        land = [country_grid[i] for i in area if country_grid[i] in country_of]
        if land:
            main = max(set(land), key=land.count)
            if land.count(main) / country_cells[main] >= DISPUTED_WHOLE_COUNTRY:
                kept.append(name)
                continue
        for i in area:
            p.grid[i] = 0
        blanked.append(name)
    stats["disputed blank"] = len(blanked)
    stats["disputed kept"] = len(kept)
    disputed_names = sorted(set(blanked) - {""})
    kept_names = sorted(set(kept) - {""})

    # Only the names still on the grid
    used = sorted(set(p.grid) - {0})
    remap = {old: new for new, old in enumerate(used, start=1)}
    names = [["", ""]] + [list(p.names[old]) for old in used]
    grid = array.array("H", (remap.get(v, 0) for v in p.grid))

    # Run-length rows: header, an offset per row, then (length, id) pairs, all little-endian
    rows = []
    for r in range(ROWS):
        row = grid[r * COLS:(r + 1) * COLS]
        runs = bytearray()
        start = 0
        for c in range(1, COLS + 1):
            if c == COLS or row[c] != row[start]:
                runs += struct.pack("<HH", c - start, row[start])
                start = c
        rows.append(bytes(runs))
    header = b"ISSP" + struct.pack("<HHHH", 1, COLS, ROWS, 0)
    offsets, pos = [], len(header) + 4 * ROWS
    for row in rows:
        offsets.append(pos)
        pos += len(row)
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "places.bin"), "wb") as f:
        f.write(header + struct.pack("<%dI" % ROWS, *offsets) + b"".join(rows))
    with open(os.path.join(out_dir, "places.json"), "w", encoding="utf-8") as f:
        json.dump({
            "about": "Place names under the ISS for webpage2/api/iss.php. Made by scripts/make_iss_places.py from "
                     "Natural Earth 5.1.2 (public domain). Index 0 is no place.",
            "cell_deg": CELL_DEG, "cols": COLS, "rows": ROWS,
            "sources": sources,
            "disputed_blank": disputed_names,
            "disputed_whole_countries_named": kept_names,
            "names": names,
        }, f, ensure_ascii=False, indent=0)
        f.write("\n")
    print(f"{len(names) - 1} names, {pos} bytes of grid; " + ", ".join(f"{k} {v}" for k, v in stats.items()))


if __name__ == "__main__":
    main()
