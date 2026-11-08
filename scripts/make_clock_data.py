#!/usr/bin/env python3
"""Builds the world clock's city and time zone table: sketch/sketch_jan12a/clock_data.h

Cities: Natural Earth populated places, 10m (public domain), from
https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_10m_populated_places.geojson
Kept: every national capital, every place with at least 3 million people, Swiss places with at least
50,000 and German and Austrian ones with at least 500,000.

Time zones: Natural Earth's TIMEZONE field (IANA names). Where it is empty, ZONE_FIXES below names the
zone by hand. Each zone's rules come from the IANA tz database as installed (TZif files end with the
zone's POSIX TZ string, RFC 8536 section 3.3), so daylight saving time follows the real rules.

Usage: python3 -I scripts/make_clock_data.py <ne_10m_populated_places.geojson> <zoneinfo dir> <out.h>
"""
import hashlib
import json
import os
import sys
import unicodedata

# Places whose TIMEZONE field is empty in Natural Earth. Each is the zone the city itself lies in.
ZONE_FIXES = {
    ("Irvine", "US"): "America/Los_Angeles",
    ("Hechi", "CN"): "Asia/Shanghai",
    ("Bir Lehlou", "EH"): "Africa/El_Aaiun",
    ("New Taipei", "TW"): "Asia/Taipei",
    ("Sanaa", "YE"): "Asia/Aden",
    ("Damascus", "SY"): "Asia/Damascus",
    ("Tripoli", "LY"): "Africa/Tripoli",
    ("Houston", "US"): "America/Chicago",
    ("Melbourne", "AU"): "Australia/Melbourne",
    ("Moscow", "RU"): "Europe/Moscow",
    ("Sydney", "AU"): "Australia/Sydney",
}

# Extra names people use, written by hand. Folded like every other key.
ALIASES = {
    "New York": ["New York City", "NYC"],
    "Washington": ["Washington DC", "Washington D.C."],
    "Beijing": ["Peking"],
    "Mumbai": ["Bombay"],
    "Kolkata": ["Calcutta"],
    "Chennai": ["Madras"],
    "Ho Chi Minh City": ["Saigon"],
    "Yangon": ["Rangoon"],
    "Mexico City": ["Ciudad de Mexico"],
}

# The fold the firmware does (clock_core.h). Letters U+00C0..U+017F map to plain letters.
SPECIAL = {"ß": "ss", "Æ": "ae", "æ": "ae", "Œ": "oe", "œ": "oe", "Ø": "o", "ø": "o", "Đ": "d", "đ": "d",
           "Ł": "l", "ł": "l", "ı": "i", "Þ": "th", "þ": "th", "Ð": "d", "ð": "d", "Ŀ": "l", "ŀ": "l",
           "Ħ": "h", "ħ": "h", "Ŧ": "t", "ŧ": "t", "ĸ": "k", "Ŋ": "n", "ŋ": "n", "ſ": "s", "×": "", "÷": "",
           "Ĳ": "ij", "ĳ": "ij", "ŉ": "n"}


def fold_letter(ch):
    if ch in SPECIAL:
        return SPECIAL[ch]
    base = unicodedata.normalize("NFKD", ch)
    out = "".join(b for b in base if "a" <= b.lower() <= "z")
    return out.lower()


FOLD_TABLE = [fold_letter(chr(cp)) for cp in range(0xC0, 0x180)]


def fold(name):
    out = []
    for ch in name:
        cp = ord(ch)
        if "a" <= ch <= "z":
            out.append(ch)
        elif "A" <= ch <= "Z":
            out.append(ch.lower())
        elif 0xC0 <= cp < 0x180:
            out.append(FOLD_TABLE[cp - 0xC0])
    return "".join(out)


def fold_german(name):
    """Zuerich for Zürich, as people type it without umlauts."""
    for a, b in (("ä", "ae"), ("ö", "oe"), ("ü", "ue"), ("Ä", "Ae"), ("Ö", "Oe"), ("Ü", "Ue")):
        name = name.replace(a, b)
    return fold(name)


def screen_name(name):
    """Keeps what the stick's fonts can show (up to U+017F); other letters lose their marks."""
    name = " ".join(name.split())
    out = []
    for ch in name:
        if ord(ch) < 0x180:
            out.append(ch)
        elif ch in "ʻʼ‘’":
            out.append("'")
        else:
            out.append("".join(b for b in unicodedata.normalize("NFKD", ch) if ord(b) < 0x180))
    return "".join(out)


def posix_rule(zoneinfo, zone):
    path = os.path.join(zoneinfo, zone)
    with open(path, "rb") as f:
        data = f.read()
    if not data.startswith(b"TZif"):
        raise SystemExit(f"{zone}: not a TZif file")
    tail = data.rstrip(b"\n").rsplit(b"\n", 1)[-1].decode("ascii")
    if not tail or tail.startswith("TZif"):
        raise SystemExit(f"{zone}: no POSIX rule at the end of the file")
    return tail


def keep(p):
    if p["FEATURECLA"] in ("Admin-0 capital", "Admin-0 capital alt"):
        return True
    pop = p["POP_MAX"] or 0
    if pop >= 3_000_000:
        return True
    if p["ISO_A2"] == "CH" and pop >= 50_000:
        return True
    if p["ISO_A2"] in ("DE", "AT") and pop >= 500_000:
        return True
    return False


def c_string(s):
    """A C literal. Non-ASCII bytes as three-digit octal escapes, which never swallow the next character."""
    out = []
    for b in s.encode("utf-8"):
        if b == 0x22 or b == 0x5C:
            out.append("\\" + chr(b))
        elif 0x20 <= b < 0x7F:
            out.append(chr(b))
        else:
            out.append("\\%03o" % b)
    return '"' + "".join(out) + '"'


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    source, zoneinfo, out_path = sys.argv[1:]
    with open(source, "rb") as f:
        raw = f.read()
    digest = hashlib.sha256(raw).hexdigest()
    version_path = os.path.join(zoneinfo, "+VERSION")
    tz_version = open(version_path).read().strip() if os.path.exists(version_path) else "unknown"
    places = [feat["properties"] for feat in json.loads(raw)["features"]]
    chosen = [p for p in places if keep(p)]
    chosen.sort(key=lambda p: (-(p["POP_MAX"] or 0), p["NAME"]))

    zones = []
    zone_index = {}
    cities = []
    for p in chosen:
        zone = p["TIMEZONE"] or ZONE_FIXES.get((p["NAME"], p["ISO_A2"]))
        if not zone:
            raise SystemExit(f"{p['NAME']} ({p['ISO_A2']}): no time zone, add it to ZONE_FIXES")
        if zone not in zone_index:
            zone_index[zone] = len(zones)
            zones.append((zone, posix_rule(zoneinfo, zone)))
        en = screen_name(p["NAME_EN"] or p["NAME"])
        de = screen_name(p["NAME_DE"] or en)
        lat, lon = float(p["LATITUDE"]), float(p["LONGITUDE"])
        if not (-90 <= lat <= 90 and -180 <= lon <= 180):
            raise SystemExit(f"{en}: coordinates out of range")
        capital = p["FEATURECLA"] in ("Admin-0 capital", "Admin-0 capital alt")
        cities.append({"en": en, "de": de, "lat": round(lat * 100), "lon": round(lon * 100),
                       "zone": zone_index[zone], "capital": capital, "pop": p["POP_MAX"] or 0, "p": p})
    if len(zones) > 255 or len(cities) > 65535:
        raise SystemExit("too many zones or cities")

    # Keys: every name a city goes by. A key two cities share goes to the capital, else the bigger one;
    # the list is sorted biggest first, so the first claim wins unless a capital comes later.
    keys = {}
    for i, city in enumerate(cities):
        p = city["p"]
        names = [p["NAME"], p["NAMEASCII"], p["NAME_EN"], p["NAME_DE"], city["en"], city["de"]]
        names += [n for n in (p["NAMEALT"] or "").split("|") if len(fold(n)) >= 4]
        names += ALIASES.get(p["NAME_EN"] or p["NAME"], []) + ALIASES.get(p["NAME"], [])
        folded = set()
        for n in names:
            if not n:
                continue
            folded.add(fold(n))
            folded.add(fold_german(n))
        for k in folded:
            if len(k) < 2:
                continue
            if k not in keys:
                keys[k] = i
            elif city["capital"] and not cities[keys[k]]["capital"]:
                keys[k] = i
    for alias_of in ALIASES:
        if not any(c["p"]["NAME_EN"] == alias_of or c["p"]["NAME"] == alias_of for c in cities):
            raise SystemExit(f"alias target {alias_of} not in the table")
    home = fold("Zurich")
    if home not in keys:
        raise SystemExit("Zurich, the default home city, is missing")

    lines = []
    lines.append("// Generated by scripts/make_clock_data.py. Do not edit by hand.")
    lines.append(f"// Cities: Natural Earth ne_10m_populated_places (public domain), sha256 {digest}")
    lines.append(f"// Time zones: IANA tz database {tz_version}, POSIX rules from the TZif files")
    lines.append(f"// {len(cities)} cities, {len(zones)} zones, {len(keys)} names")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("struct ClockZone {")
    lines.append("  const char* iana;   // e.g. Europe/Zurich")
    lines.append("  const char* posix;  // TZ rule with daylight saving time, e.g. CET-1CEST,M3.5.0,M10.5.0/3")
    lines.append("};")
    lines.append("struct ClockCity {")
    lines.append("  const char* en;  // Name in English, UTF-8")
    lines.append("  const char* de;  // Name in German, UTF-8")
    lines.append("  int16_t lat100, lon100;  // Degrees x 100")
    lines.append("  uint8_t zone;            // Index into CLOCK_ZONES")
    lines.append("  uint8_t capital;         // 1: national capital")
    lines.append("};")
    lines.append("struct ClockKey {")
    lines.append("  const char* key;  // Folded name: lower-case a-z only")
    lines.append("  uint16_t city;")
    lines.append("};")
    lines.append("")
    lines.append("// What letters U+00C0 to U+017F fold to, in order")
    lines.append("static const char* const CLOCK_FOLD_LATIN[%d] = {" % len(FOLD_TABLE))
    for i in range(0, len(FOLD_TABLE), 16):
        lines.append("    " + ", ".join(c_string(s) for s in FOLD_TABLE[i:i + 16]) + ",")
    lines.append("};")
    lines.append("")
    lines.append("static const ClockZone CLOCK_ZONES[] = {")
    for iana, posix in zones:
        lines.append(f"    {{{c_string(iana)}, {c_string(posix)}}},")
    lines.append("};")
    lines.append(f"static const int CLOCK_ZONE_COUNT = {len(zones)};")
    lines.append("")
    lines.append("// Biggest first")
    lines.append("static const ClockCity CLOCK_CITIES[] = {")
    for c in cities:
        lines.append(f"    {{{c_string(c['en'])}, {c_string(c['de'])}, {c['lat']}, {c['lon']}, {c['zone']}, "
                     f"{1 if c['capital'] else 0}}},")
    lines.append("};")
    lines.append(f"static const int CLOCK_CITY_COUNT = {len(cities)};")
    lines.append(f"static const int CLOCK_HOME_DEFAULT = {keys[home]};  // Zurich")
    lines.append("")
    lines.append("// Sorted by key, for a binary search")
    lines.append("static const ClockKey CLOCK_KEYS[] = {")
    for k in sorted(keys):
        lines.append(f"    {{{c_string(k)}, {keys[k]}}},")
    lines.append("};")
    lines.append(f"static const int CLOCK_KEY_COUNT = {len(keys)};")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{len(cities)} cities, {len(zones)} zones, {len(keys)} keys -> {out_path}")


if __name__ == "__main__":
    main()
