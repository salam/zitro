#!/usr/bin/env python3
"""Builds the globe's coastline files from Natural Earth (public domain).

Download the two inputs first (curl, because Python's certificate store doesn't work here):
  B=https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson
  curl -sL -o DIR/ne_110m_coastline.geojson $B/ne_110m_coastline.geojson
  curl -sL -o DIR/ne_50m_coastline.geojson  $B/ne_50m_coastline.geojson
Then: python3 -I scripts/make_globe_data.py DIR

Writes, in the same format (parser: globeCheckFile in globe_core.h):
  sketch/sketch_jan12a/globe_coast_data.h   Built in: Natural Earth 110m, every point
  webpage2/data/globe/coast_detail.bin      On the server: 50m, thinned to DETAIL_POINTS

Format, little endian:
  "ZGLB", version u8 = 1, reserved u8 = 0, lines u16, points u16
  points in each line: u16 * lines
  latitude, longitude in 1/100 degree: i16, i16 * points
"""
import hashlib
import json
import math
import os
import struct
import sys

DETAIL_POINTS = 15000
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER_OUT = os.path.join(ROOT, "sketch", "sketch_jan12a", "globe_coast_data.h")
DETAIL_OUT = os.path.join(ROOT, "webpage2", "data", "globe", "coast_detail.bin")


def read_lines(path):
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    lines = []
    for feature in data["features"]:
        geometry = feature["geometry"]
        if geometry["type"] == "LineString":
            lines.append(geometry["coordinates"])
        elif geometry["type"] == "MultiLineString":
            lines.extend(geometry["coordinates"])
        else:
            raise SystemExit("unexpected geometry " + geometry["type"])
    for line in lines:
        for a, b in zip(line, line[1:]):
            # A jump across the date line would draw a stripe across the whole globe
            if abs(a[0] - b[0]) > 180:
                raise SystemExit("segment jumps across the date line")
    return lines


def unit(point):
    lon, lat = math.radians(point[0]), math.radians(point[1])
    return (math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon), math.sin(lat))


def distance_to_chord(p, a, b):
    ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    ap = (p[0] - a[0], p[1] - a[1], p[2] - a[2])
    length2 = ab[0] ** 2 + ab[1] ** 2 + ab[2] ** 2
    if length2 == 0:
        return math.sqrt(ap[0] ** 2 + ap[1] ** 2 + ap[2] ** 2)
    t = max(0.0, min(1.0, (ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2]) / length2))
    d = (ap[0] - t * ab[0], ap[1] - t * ab[1], ap[2] - t * ab[2])
    return math.sqrt(d[0] ** 2 + d[1] ** 2 + d[2] ** 2)


def error_tree(vectors):
    """Douglas-Peucker once, keeping each point's split error. Thinning is then a threshold on it."""
    errors = [0.0] * len(vectors)
    errors[0] = errors[-1] = math.inf
    if vectors[0] == vectors[-1] and len(vectors) > 3:
        # Closed ring: its two ends are one point, so also keep the point farthest from them
        far = max(range(1, len(vectors) - 1), key=lambda i: distance_to_chord(vectors[i], vectors[0], vectors[0]))
        errors[far] = math.inf
        stack = [(0, far), (far, len(vectors) - 1)]
    else:
        stack = [(0, len(vectors) - 1)]
    while stack:
        first, last = stack.pop()
        if last - first < 2:
            continue
        best, index = -1.0, first + 1
        for i in range(first + 1, last):
            d = distance_to_chord(vectors[i], vectors[first], vectors[last])
            if d > best:
                best, index = d, i
        errors[index] = best
        stack.append((first, index))
        stack.append((index, last))
    return errors


def thin(lines, budget):
    """Same tolerance everywhere, the smallest that fits the budget. Islands too small to see drop out."""
    trees = []
    for line in lines:
        vectors = [unit(p) for p in line]
        size = max((distance_to_chord(v, vectors[0], vectors[0]) for v in vectors), default=0.0)
        trees.append((line, error_tree(vectors), size))

    def pick(tolerance):
        out = []
        for line, errors, size in trees:
            if size < tolerance:
                continue
            kept = [p for p, e in zip(line, errors) if e >= tolerance]
            if len(kept) >= 2:
                out.append(kept)
        return out

    low, high = 0.0, 0.2
    for _ in range(50):
        middle = (low + high) / 2
        if sum(len(x) for x in pick(middle)) > budget:
            low = middle
        else:
            high = middle
    return pick(high), high


def encode(lines):
    rounded = []
    for line in lines:
        points = []
        for lon, lat in line:
            point = (round(lat * 100), round(lon * 100))
            if not points or points[-1] != point:
                points.append(point)
        if len(points) >= 2:
            rounded.append(points)
    total = sum(len(x) for x in rounded)
    out = bytearray(b"ZGLB" + struct.pack("<BBHH", 1, 0, len(rounded), total))
    for points in rounded:
        out += struct.pack("<H", len(points))
    for points in rounded:
        for lat, lon in points:
            if abs(lat) > 9000 or abs(lon) > 18000:
                raise SystemExit("coordinate out of range")
            out += struct.pack("<hh", lat, lon)
    return bytes(out), len(rounded), total


def write_header(data, lines, points, source_hash):
    with open(HEADER_OUT, "w", encoding="ascii") as f:
        f.write("// Generated by scripts/make_globe_data.py. Do not edit.\n")
        f.write("// Natural Earth 110m coastline, public domain. Input sha256 %s\n" % source_hash)
        f.write("#pragma once\n\n#include <stdint.h>\n\n")
        f.write("// %d lines, %d points, %d bytes\n" % (lines, points, len(data)))
        f.write("static const uint32_t GLOBE_COAST_BUILTIN_SIZE = %d;\n" % len(data))
        f.write("static const uint8_t GLOBE_COAST_BUILTIN[] = {\n")
        for i in range(0, len(data), 20):
            f.write("  " + ",".join(str(b) for b in data[i : i + 20]) + ",\n")
        f.write("};\n")


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    path110 = os.path.join(sys.argv[1], "ne_110m_coastline.geojson")
    path50 = os.path.join(sys.argv[1], "ne_50m_coastline.geojson")

    data, lines, points = encode(read_lines(path110))
    write_header(data, lines, points, sha256(path110))
    print("built in: %d lines, %d points, %d bytes" % (lines, points, len(data)))

    thinned, tolerance = thin(read_lines(path50), DETAIL_POINTS)
    data, lines, points = encode(thinned)
    os.makedirs(os.path.dirname(DETAIL_OUT), exist_ok=True)
    with open(DETAIL_OUT, "wb") as f:
        f.write(data)
    print("server: %d lines, %d points, %d bytes, tolerance %.1f km, input sha256 %s"
          % (lines, points, len(data), tolerance * 6371, sha256(path50)))


if __name__ == "__main__":
    main()
