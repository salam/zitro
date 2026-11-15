#!/usr/bin/env python3
"""Brings the time zone rules up to date with the installed tz database, without touching anything else.

When a country changes its clocks (Cuba kept summer time through the winter of 2026/27), the stick's table
(sketch/sketch_jan12a/clock_data.h) and the server's (webpage2/api/places/zones.tsv) need the new rules and
exceptions. Regenerating them in full would also need the original city downloads and could renumber the zones,
which the place shards and the IP locator's data refer to. This rewrites only each zone's rule and exceptions,
in place and in order, with the functions make_clock_data.py uses.

Usage: python3 -I scripts/update_zone_rules.py <clock_data.h> <zones.tsv> <zoneinfo dir> [YYYY-MM-DD]
The date is where exceptions start counting (default: today).
"""
import calendar
import importlib.util
import os
import re
import sys
import time

_spec = importlib.util.spec_from_file_location(
    "make_clock_data", os.path.join(os.path.dirname(os.path.abspath(__file__)), "make_clock_data.py"))
_clock = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_clock)


def main():
    if len(sys.argv) not in (4, 5):
        raise SystemExit(__doc__)
    header_path, tsv_path, zoneinfo = sys.argv[1:4]
    start = calendar.timegm(time.strptime(sys.argv[4], "%Y-%m-%d")) if len(sys.argv) == 5 else \
        int(time.time()) // 86400 * 86400
    version_path = os.path.join(zoneinfo, "+VERSION")
    tz_version = open(version_path).read().strip() if os.path.exists(version_path) else "unknown"

    def rules(zone):
        rule = _clock.posix_rule(zoneinfo, zone)
        return rule, _clock.zone_until(zoneinfo, zone, rule, start)

    text = open(header_path, encoding="utf-8").read()
    begin = text.index("static const ClockZone CLOCK_ZONES[] = {\n") + len("static const ClockZone CLOCK_ZONES[] = {\n")
    end = text.index("};", begin)
    names = re.findall(r'^\s*\{"([^"]+)"', text[begin:end], re.M)
    zones = [(name,) + rules(name) for name in names]
    text = text[:begin] + "\n".join(_clock.zone_rows(zones)) + "\n" + text[end:]
    text = re.sub(r"^// Time zones: IANA tz database .*$",
                  f"// Time zones: IANA tz database {tz_version}, POSIX rules from the TZif files" + _clock.zone_note(start),
                  text, count=1, flags=re.M)
    with open(header_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)

    lines = []
    for line in open(tsv_path, encoding="utf-8"):
        zone = line.split("\t")[0]
        rule, until = rules(zone)
        lines.append(f"{zone}\t{rule}" + (f"\t{until}" if until else "") + "\n")
    with open(tsv_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(lines)
    changed = [z for z in zones if z[2]]
    print(f"tz {tz_version}: {len(zones)} stick zones, {len(lines)} server zones; exceptions: "
          + (", ".join(z[0] for z in changed) or "none"))


if __name__ == "__main__":
    main()
