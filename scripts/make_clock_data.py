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
import struct
import sys
import time
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


def tzif_transitions(path):
    """The explicit transitions of a TZif v2+ file (RFC 8536): UTC times and the UTC offset from each on."""
    with open(path, "rb") as f:
        data = f.read()
    if not data.startswith(b"TZif") or data[4:5] == b"\0":
        raise SystemExit(f"{path}: not a TZif file of version 2 or later")

    def counts(at):
        return struct.unpack(">6l", data[at + 20:at + 44])  # isut, isstd, leap, time, type, char

    isut, isstd, leap, times, types, chars = counts(0)
    at = 44 + times * 5 + types * 6 + chars + leap * 8 + isstd + isut  # Past the 32-bit block
    isut, isstd, leap, times, types, chars = counts(at)
    at += 44
    when = struct.unpack(f">{times}q", data[at:at + 8 * times])
    at += 8 * times
    index = data[at:at + times]
    at += times
    offsets = [struct.unpack(">lBB", data[at + 6 * i:at + 6 * i + 6])[0] for i in range(types)]
    return list(when), [offsets[i] for i in index]


def _days_from_civil(y, m, d):
    y -= m <= 2
    era = y // 400
    yoe = y - era * 400
    doy = (153 * (m + (-3 if m > 2 else 9)) + 2) // 5 + d - 1
    doe = yoe * 365 + yoe // 4 - yoe // 100 + doy
    return era * 146097 + doe - 719468


def _parse_posix(rule):
    """A POSIX TZ string as clock_core.h reads it: (std offset, dst offset or None, start, end), offsets east."""
    pos = 0

    def name():
        nonlocal pos
        if rule[pos] == "<":
            pos = rule.index(">", pos) + 1
        else:
            while pos < len(rule) and rule[pos].isalpha():
                pos += 1

    def seconds():
        nonlocal pos
        sign = 1
        if rule[pos] in "+-":
            sign = -1 if rule[pos] == "-" else 1
            pos += 1
        parts = [0, 0, 0]
        for i in range(3):
            start = pos
            while pos < len(rule) and rule[pos].isdigit():
                pos += 1
            parts[i] = int(rule[start:pos])
            if i < 2 and pos < len(rule) and rule[pos] == ":":
                pos += 1
            else:
                break
        return sign * (parts[0] * 3600 + parts[1] * 60 + parts[2])

    def transition():
        nonlocal pos
        end = rule.find(",", pos)
        part = rule[pos:] if end < 0 else rule[pos:end]
        pos += len(part)
        date, _, time = part.partition("/")
        at = 7200
        if time:
            sign = -1 if time.startswith("-") else 1
            fields = [int(x) for x in time.lstrip("+-").split(":")] + [0, 0]
            at = sign * (fields[0] * 3600 + fields[1] * 60 + fields[2])
        return date, at

    name()
    std = -seconds()
    if pos >= len(rule):
        return std, None, None, None
    name()
    dst = std + 3600
    if rule[pos] != ",":
        dst = -seconds()
    pos += 1
    start = transition()
    pos += 1
    end = transition()
    return std, dst, start, end


def _transition_day(date, year):
    if date.startswith("M"):
        month, week, weekday = (int(x) for x in date[1:].split("."))
        first = _days_from_civil(year, month, 1)
        day = first + (weekday - (first + 4) % 7) % 7 + 7 * (week - 1)
        length = [31, 28 + (year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)), 31, 30, 31, 30, 31, 31, 30, 31,
                  30, 31][month - 1]
        while day >= first + length:
            day -= 7
        return day
    if date.startswith("J"):
        n = int(date[1:])
        leap = year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)
        return _days_from_civil(year, 1, 1) + n - 1 + (1 if leap and n >= 60 else 0)
    return _days_from_civil(year, 1, 1) + int(date)


def posix_changes(rule, first_year, last_year):
    """The UTC times at which a POSIX rule changes the offset, and the offset from then on."""
    std, dst, start, end = _parse_posix(rule)
    if dst is None:
        return []
    changes = []
    for year in range(first_year, last_year + 1):
        changes.append((_transition_day(start[0], year) * 86400 + start[1] - std, dst))
        changes.append((_transition_day(end[0], year) * 86400 + end[1] - dst, std))
    return sorted(changes)


def _offset_at(changes, before, t):
    offset = before
    for when, after in changes:
        if when > t:
            break
        offset = after
    return offset


EXCEPTION_YEARS = 6  # How far ahead exceptions are kept: the data is made again long before that


def zone_until(zoneinfo, zone, rule, start):
    """Where the tz database's own transitions in the next EXCEPTION_YEARS after `start` differ from what the
    zone's POSIX rule says (as Cuba keeping summer time through the winter of 2026/27): the offsets the stick
    uses instead, as "until:offset;until:offset" (UTC seconds, seconds east). Before each `until` its offset
    holds; from the last one on, the rule. Empty when the rule is right all that time.
    """
    when, offsets = tzif_transitions(os.path.join(zoneinfo, zone))
    horizon = start + EXCEPTION_YEARS * 365 * 86400
    if not when or when[-1] <= start:
        return ""
    explicit = list(zip(when, offsets))
    last = min(when[-1], horizon)
    years = range(time.gmtime(start).tm_year - 1, time.gmtime(last).tm_year + 2)
    rule_changes = posix_changes(rule, years[0], years[-1])
    std = _parse_posix(rule)[0]
    breaks = sorted({start} | {t for t, _ in explicit if start < t <= last} | {t for t, _ in rule_changes if start < t <= last})
    rule_before = _offset_at(rule_changes, std, start - 400 * 86400)
    tz_before = offsets[0]
    wrong = [t for t in breaks if _offset_at(explicit, tz_before, t) != _offset_at(rule_changes, rule_before, t)]
    if not wrong:
        return ""
    # From the first break after the last wrong one, the rule is right for good
    right_from = min(t for t in breaks + [last + 1] if t > wrong[-1])
    segments, at = [], start
    for t, _ in explicit:
        if at < t < right_from:
            segments.append((t, _offset_at(explicit, tz_before, at)))
            at = t
    segments.append((right_from, _offset_at(explicit, tz_before, at)))
    if len(segments) > 4:
        raise SystemExit(f"{zone}: {len(segments)} exceptions to its rule, the stick keeps 4")
    return ";".join(f"{u}:{o}" for u, o in segments)


def zone_note(start):
    return f", exceptions from {time.strftime('%Y-%m-%d', time.gmtime(start))}"


def zone_rows(zones):
    """CLOCK_ZONES' rows: {iana, posix} or, with exceptions to the rule, {iana, posix, until}"""
    rows = []
    for iana, posix, until in zones:
        extra = f", {c_string(until)}" if until else ""
        rows.append(f"    {{{c_string(iana)}, {c_string(posix)}{extra}}},")
    return rows


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
    start = int(time.time()) // 86400 * 86400  # Exceptions to a zone's rule count from today
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
            rule = posix_rule(zoneinfo, zone)
            zones.append((zone, rule, zone_until(zoneinfo, zone, rule, start)))
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
    lines.append(f"// Time zones: IANA tz database {tz_version}, POSIX rules from the TZif files" + zone_note(start))
    lines.append(f"// {len(cities)} cities, {len(zones)} zones, {len(keys)} names")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("struct ClockZone {")
    lines.append("  const char* iana;   // e.g. Europe/Zurich")
    lines.append("  const char* posix;  // TZ rule with daylight saving time, e.g. CET-1CEST,M3.5.0,M10.5.0/3")
    lines.append("  const char* until;  // Fixed offsets before the rule, see clockParseUntil: usually \"\"")
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
    lines += zone_rows(zones)
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
