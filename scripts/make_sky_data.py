#!/usr/bin/env python3
"""Generates sketch/sketch_jan12a/sky_data.h: stars, constellation lines and names for the sky view.

Usage: python3 -I scripts/make_sky_data.py <download dir> <output header>

The download dir holds these untouched files (see docs/features/sky.md for sources and licences):
  bsc5p.tdat.gz              https://heasarc.gsfc.nasa.gov/FTP/heasarc/dbase/tdat_files/heasarc_bsc5p.tdat.gz
  constellations.lines.json  https://raw.githubusercontent.com/ofrohn/d3-celestial/master/data/constellations.lines.json
  constellations.json        https://raw.githubusercontent.com/ofrohn/d3-celestial/master/data/constellations.json
  IAU-CSN.txt                https://www.pas.rochester.edu/~emamajek/WGSN/IAU-CSN.txt

Fails loudly on anything unexpected: a line point without a catalogue star, a query key that
would mean two constellations, a character the stick's font can't draw.
"""
import gzip
import hashlib
import json
import math
import os
import re
import sys

MAG_LIMIT = 5.0        # Stars down to this magnitude; dimmer ones only if a constellation line ends there
MATCH_LIMIT_DEG = 0.01  # A line point must be this close to a catalogue star
MAIN_STARS = 3         # Named stars per constellation

# B-V colour index to a colour band: upper limit of each band. Blue-white stars have low B-V, red ones high.
BV_BANDS = [-0.1, 0.15, 0.45, 0.75, 1.1]  # Band 5 is everything above. Rough on purpose: a hint of colour
BV_NEUTRAL_BAND = 2  # White, for the few stars without a B-V value

# Corrections to the d3-celestial names. Where d3 gives a wrong or colliding English name, or an
# unusually long German one. "Big Dipper" is only part of Ursa Major, for instance, so it's an alias below.
ENGLISH_NAME = {
    "UMa": "Great Bear",
    "UMi": "Little Bear",
    "Cru": "Southern Cross",
    "Cir": "Drafting Compass",  # d3 calls both Circinus and Pyxis "Compass"
    "Pyx": "Mariner's Compass",
    "Hya": "Water Snake",
    "Hyi": "Lesser Water Snake",
    "Men": "Table Mountain",
    "Aqr": "Water Bearer",
}
GERMAN_NAME = {"Hyi": "Kleine Wasserschlange", "Car": "Schiffskiel"}

# More ways people ask for a constellation. Only ones that mean this constellation, or a part of it.
# Not "Stern" for Puppis (German for star), nor Fish, Snake or Cross, which could mean several.
ALIASES = {
    "UMa": ["Big Dipper", "Plough", "Plow", "Grosser Wagen"],
    "UMi": ["Little Dipper", "Lesser Bear", "Kleiner Wagen"],
    "Sco": ["Scorpio"],
    "Aqr": ["Water Carrier"],
    "Cap": ["Sea Goat"],
    "CMa": ["Big Dog", "Greater Dog"],
    "CMi": ["Lesser Dog"],
    "Hya": ["Sea Serpent"],
    "Cet": ["Sea Monster"],
    "Eri": ["River"],
    "Lib": ["Scales"],
    "Vir": ["Maiden"],
    "Cyg": ["Northern Cross"],
    "Ser": ["Serpens Caput", "Serpens Cauda"],
    "Oph": ["Serpent Bearer"],
    "Ori": ["Hunter", "Orions Belt"],
    "Vel": ["Sail"],
}


def fail(message):
    sys.exit("make_sky_data: " + message)


def unit(ra_deg, dec_deg):
    a, d = math.radians(ra_deg), math.radians(dec_deg)
    return (math.cos(d) * math.cos(a), math.cos(d) * math.sin(a), math.sin(d))


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def angle(a, b):
    return math.degrees(math.acos(max(-1.0, min(1.0, dot(a, b)))))


def normalised(v):
    n = math.sqrt(dot(v, v))
    return (v[0] / n, v[1] / n, v[2] / n)


def read_stars(path):
    fields, stars, in_data = [], [], False
    with gzip.open(path, "rt", errors="replace") as f:
        for line in f:
            if line.startswith("line[1]"):
                fields = line.split("=")[1].split()
            elif line.startswith("<DATA>"):
                in_data = True
            elif line.startswith("<END>"):
                break
            elif in_data:
                row = dict(zip(fields, line.rstrip("\n").split("|")))
                # The 14 non-stellar entries (novae, clusters) have no magnitude
                if row.get("hr") and row.get("vmag"):
                    stars.append({
                        "hr": int(row["hr"]),
                        "mag": float(row["vmag"]),
                        "bv": float(row["bv_color"]) if row["bv_color"] else None,
                        "v": unit(float(row["ra"]), float(row["dec"])),
                    })
    if len(stars) < 9000:
        fail("expected about 9096 catalogue stars, got %d" % len(stars))
    return stars


def read_star_names(path):
    """IAU names of stars that have an HR number: {constellation: [(hr, name)]}"""
    names = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line[0] in "#$" or not line.strip():
                continue
            hr = re.search(r"\bHR\s+(\d+)", line)
            if not hr:
                continue
            con = line[61:64]  # Fixed-width column
            if not re.fullmatch(r"[A-Z][A-Za-z]{2}", con):
                fail("no constellation in IAU line: " + line.strip())
            names.setdefault(con, []).append((int(hr.group(1)), line[:18].strip()))
    return names


def fold(text):
    """The same folding as skyFoldName() in sky_core.h: letters only, lower case, no accents."""
    text = text.replace("ä", "a").replace("ö", "o").replace("ü", "u").replace("ß", "ss")
    text = text.replace("Ä", "a").replace("Ö", "o").replace("Ü", "u")
    return re.sub(r"[ '.\-]", "", text).lower()


def clean_name(text):
    """A name as shown on screen: plain spaces only, and only what the font can draw."""
    text = " ".join(text.replace(" ", " ").split())
    for c in text:
        if not 0x20 <= ord(c) <= 0x5FF:
            fail("character U+%04X in %r is outside the font" % (ord(c), text))
    return text


def enclosing_centre(points):
    """Centre and radius of a small circle around the points (Badoiu-Clarkson on the sphere)."""
    centre = normalised(tuple(sum(p[i] for p in points) for i in range(3)))
    for step in range(1, 400):
        far = min(points, key=lambda p: dot(p, centre))
        t = 1.0 / (step + 1)
        centre = normalised(tuple(centre[i] * (1 - t) + far[i] * t for i in range(3)))
    return centre, max(angle(centre, p) for p in points)


def cstr(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def cutf8(text):
    """C string with non-ASCII bytes as octal escapes (hex escapes would swallow following letters)."""
    out = ""
    for byte in text.encode("utf-8"):
        out += "\\%03o" % byte if byte >= 0x80 else chr(byte)
    return '"' + out + '"'


def main():
    if len(sys.argv) != 3:
        fail("usage: make_sky_data.py <download dir> <output header>")
    source, output = sys.argv[1:3]
    files = ["bsc5p.tdat.gz", "constellations.lines.json", "constellations.json", "IAU-CSN.txt"]
    hashes = []
    for name in files:
        with open(os.path.join(source, name), "rb") as f:
            hashes.append("%s sha256 %s" % (name, hashlib.sha256(f.read()).hexdigest()))

    catalogue = read_stars(os.path.join(source, "bsc5p.tdat.gz"))
    by_hr = {s["hr"]: s for s in catalogue}
    star_names = read_star_names(os.path.join(source, "IAU-CSN.txt"))
    with open(os.path.join(source, "constellations.lines.json"), encoding="utf-8") as f:
        line_features = json.load(f)["features"]
    with open(os.path.join(source, "constellations.json"), encoding="utf-8") as f:
        name_features = json.load(f)["features"]

    # Serpens comes in two parts, under one id: merged here into the one IAU constellation
    polylines = {}
    for feature in line_features:
        polylines.setdefault(feature["id"], []).extend(feature["geometry"]["coordinates"])
    infos = {}
    for feature in name_features:
        infos.setdefault(feature["id"], feature["properties"])
    infos["Ser"] = dict(infos["Ser"], name="Serpens")
    if len(polylines) != 88 or set(polylines) != set(infos):
        fail("expected 88 constellations with both lines and names, got %d" % len(polylines))

    # Every line point must be a real catalogue star
    def nearest(point):
        v = unit(point[0], point[1])
        best = max(catalogue, key=lambda s: dot(s["v"], v))
        off = angle(best["v"], v)
        if off > MATCH_LIMIT_DEG:
            fail("line point %s is %.4f degrees from the nearest catalogue star" % (point, off))
        return best

    point_star = {}
    segments = {}
    for con in sorted(polylines):
        seen = set()
        segments[con] = []
        for polyline in polylines[con]:
            for p in polyline:
                point_star.setdefault((p[0], p[1]), nearest(p))
            for a, b in zip(polyline, polyline[1:]):
                ha, hb = point_star[(a[0], a[1])]["hr"], point_star[(b[0], b[1])]["hr"]
                pair = (min(ha, hb), max(ha, hb))
                if ha != hb and pair not in seen:
                    seen.add(pair)
                    segments[con].append(pair)
        if not segments[con]:
            fail(con + " has no lines")

    # The table: all stars down to the limit plus those the lines need. Brightest first, then by HR number.
    needed = {s["hr"] for s in catalogue if s["mag"] <= MAG_LIMIT}
    line_only = {s["hr"] for s in point_star.values()} - needed
    table = sorted((by_hr[hr] for hr in needed | line_only), key=lambda s: (s["mag"], s["hr"]))
    index = {s["hr"]: i for i, s in enumerate(table)}
    if len(table) > 65535:
        fail("star table too large")

    def band(bv):
        if bv is None:
            return BV_NEUTRAL_BAND
        for i, limit in enumerate(BV_BANDS):
            if bv <= limit:
                return i
        return len(BV_BANDS)

    def i16(x):
        return int(round(x * 32767))

    out = []
    out.append("// Generated by scripts/make_sky_data.py. Do not edit. Sources and licences: docs/features/sky.md")
    for h in hashes:
        out.append("// " + h)
    out += ["#pragma once", "", "#include <stdint.h>", ""]
    out.append("struct SkyStar {")
    out.append("  int16_t x, y, z;  // Unit vector * 32767, J2000: x to RA 0, z to the north celestial pole")
    out.append("  uint8_t mag10;    // (magnitude + 2) * 10")
    out.append("  uint8_t band;     // Colour band from B-V, 0 blue to 5 orange-red")
    out.append("};")
    out.append("struct SkyConstellation {")
    out.append("  const char* latin;   // IAU name, used as the English label")
    out.append("  const char* german;  // UTF-8")
    out.append("  int16_t x, y, z;     // Centre of a circle around the line stars, as in SkyStar")
    out.append("  uint8_t radiusDeg;   // Radius of that circle, rounded up")
    out.append("  uint16_t lineStart, lineCount;  // Range in SKY_LINES, in pairs")
    out.append("  uint8_t mainCount;")
    out.append("  uint16_t mainStar[%d];  // Index in SKY_STARS" % MAIN_STARS)
    out.append("  const char* mainName[%d];  // IAU star name, ASCII" % MAIN_STARS)
    out.append("};")
    out.append("struct SkyKey {")
    out.append("  const char* key;  // Folded by skyFoldName, sorted")
    out.append("  uint8_t constellation;")
    out.append("};")
    out.append("")
    out.append("static const int SKY_STAR_COUNT = %d;" % len(table))
    out.append("static const float SKY_MAG_LIMIT = %.1ff;" % MAG_LIMIT)
    out.append("static const SkyStar SKY_STARS[] = {")
    for s in table:
        mag10 = int(round((s["mag"] + 2) * 10))
        out.append("{%d,%d,%d,%d,%d}," % (i16(s["v"][0]), i16(s["v"][1]), i16(s["v"][2]), mag10, band(s["bv"])))
    out += ["};", ""]

    constellations = sorted(polylines, key=lambda c: infos[c]["name"])
    line_rows, con_rows, keys = [], [], {}
    start = 0
    for number, con in enumerate(constellations):
        info = infos[con]
        latin = clean_name(info["name"])
        german = clean_name(GERMAN_NAME.get(con, info["de"]))
        english = clean_name(ENGLISH_NAME.get(con, info["en"]))
        hrs = sorted({hr for pair in segments[con] for hr in pair})
        centre, radius = enclosing_centre([by_hr[hr]["v"] for hr in hrs])
        pairs = sorted(segments[con])
        line_rows.append("".join("%d,%d," % (index[a], index[b]) for a, b in pairs))
        named = [(by_hr[hr]["mag"], hr, name) for hr, name in star_names.get(con, []) if hr in index]
        named.sort()
        main = named[:MAIN_STARS]
        for _, _, name in main:
            if not name.isascii():
                fail("star name %r is not ASCII" % name)
        stars = [str(index[hr]) for _, hr, _ in main] + ["0"] * (MAIN_STARS - len(main))
        names = [cstr(name) for _, _, name in main] + ["nullptr"] * (MAIN_STARS - len(main))
        con_rows.append('{%s,%s,%d,%d,%d,%d,%d,%d,%d,{%s},{%s},},' % (
            cstr(latin) if latin.isascii() else cutf8(latin), cutf8(german), i16(centre[0]), i16(centre[1]),
            i16(centre[2]), math.ceil(radius), start, len(pairs), len(main), ",".join(stars), ",".join(names)))
        start += len(pairs)
        candidates = [latin, english, german] + ALIASES.get(con, [])
        # German spelled in ASCII, as a plain-ASCII prompt would write it
        candidates.append(german.replace("ä", "ae").replace("ö", "oe").replace("ü", "ue"))
        for candidate in candidates:
            key = fold(candidate)
            if not key.isascii() or not key.isalpha():
                fail("key %r can't be matched" % key)
            if keys.get(key, number) != number:
                fail("key %r means both %s and %s" % (key, constellations[keys[key]], con))
            keys[key] = number
    if "all" in keys:
        fail("key 'all' is reserved for the whole dome")

    out.append("static const uint16_t SKY_LINES[] = {")
    out += line_rows
    out += ["};", ""]
    out.append("static const int SKY_LINE_COUNT = %d;" % start)
    out.append("static const int SKY_MAIN_STARS = %d;" % MAIN_STARS)
    out.append("static const int SKY_CONSTELLATION_COUNT = %d;" % len(con_rows))
    out.append("static const SkyConstellation SKY_CONSTELLATIONS[] = {")
    out += con_rows
    out += ["};", ""]
    out.append("static const int SKY_KEY_COUNT = %d;" % len(keys))
    out.append("static const SkyKey SKY_KEYS[] = {")
    for key in sorted(keys):
        out.append("{%s,%d}," % (cstr(key), keys[key]))
    out += ["};", ""]
    with open(output, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(out))

    size = len(table) * 8 + start * 4 + len(con_rows) * 44 + len(keys) * 20
    print("stars %d (%d only for lines), segments %d, constellations %d, keys %d, about %d KB of flash" % (
        len(table), len(line_only), start, len(con_rows), len(keys), size // 1024))


if __name__ == "__main__":
    main()
