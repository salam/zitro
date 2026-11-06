#!/usr/bin/env python3
"""Checks sketch/sketch_jan12a/gif_decode.h against Pillow, frame by frame.

Writes GIFs with a small encoder of its own (so every feature can be set exactly), plays them through
GifAnimator and compares each picture with what Pillow shows, both reduced to RGB565 over black.

    python3 tests/gif/run.py            run the checks
    python3 tests/gif/run.py out_dir    also keep the generated GIFs in out_dir
"""
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
BLACK, RED, GREEN, BLUE, WHITE, YELLOW, CYAN, PINK = range(8)
PALETTE = [(0, 0, 0), (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255), (255, 255, 0), (0, 255, 255), (255, 0, 255)]


def lzw(pixels, min_code):
    """GIF LZW, with data sub-blocks."""
    clear, end = 1 << min_code, (1 << min_code) + 1
    codes, table, size, nxt = [clear], {bytes([i]): i for i in range(clear)}, min_code + 1, end + 1
    out, bits, buf = bytearray(), 0, 0

    def emit(code, width):
        nonlocal bits, buf
        buf |= code << bits
        bits += width
        while bits >= 8:
            out.append(buf & 255)
            buf >>= 8
            bits -= 8

    emit(clear, size)
    cur = b""
    for p in pixels:
        nxt_seq = cur + bytes([p])
        if nxt_seq in table:
            cur = nxt_seq
            continue
        emit(table[cur], size)
        if nxt < 4096:
            table[nxt_seq] = nxt
            if nxt == (1 << size) and size < 12:
                size += 1
            nxt += 1
        else:
            emit(clear, size)
            table, size, nxt = {bytes([i]): i for i in range(clear)}, min_code + 1, end + 1
        cur = bytes([p])
    if cur:
        emit(table[cur], size)
    emit(end, size)
    if bits:
        out.append(buf & 255)
    blocks = bytearray([min_code])
    for i in range(0, len(out), 255):
        part = out[i:i + 255]
        blocks += bytes([len(part)]) + part
    return bytes(blocks) + b"\0"


def palette_bytes(colours, size=8):
    colours = list(colours) + [(0, 0, 0)] * (size - len(colours))
    return b"".join(bytes(c) for c in colours)


def interlace(rows):
    out = []
    for start, step in ((0, 8), (4, 8), (2, 4), (1, 2)):
        out += rows[start::step]
    return out


def gif(w, h, frames, loops=0):
    """frames: dicts with rows (or colour), x, y, w, h, delay, disposal, transparent, palette, interlaced."""
    o = bytearray(b"GIF89a" + struct.pack("<HH", w, h) + bytes([0xF0 | 2, 0, 0]) + palette_bytes(PALETTE))
    if loops is not None:
        o += b"\x21\xFF\x0BNETSCAPE2.0\x03\x01" + struct.pack("<H", loops) + b"\0"
    for f in frames:
        fw, fh = f.get("w", w), f.get("h", h)
        rows = f["rows"] if "rows" in f else [[f["colour"]] * fw for _ in range(fh)]
        flags = (f.get("disposal", 1) << 2) | (1 if "transparent" in f else 0)
        o += b"\x21\xF9\x04" + bytes([flags]) + struct.pack("<H", f.get("delay", 10)) + bytes([f.get("transparent", 0), 0])
        packed = 0x40 if f.get("interlaced") else 0
        local = b""
        if "palette" in f:
            packed |= 0x80 | 2
            local = palette_bytes(f["palette"])
        o += b"\x2C" + struct.pack("<HHHH", f.get("x", 0), f.get("y", 0), fw, fh) + bytes([packed]) + local
        if f.get("interlaced"):
            rows = interlace(rows)
        o += lzw(bytes(p for r in rows for p in r), 3)
    return bytes(o + b"\x3B")


def pattern(w, h, seed=0):
    """Blocks of colour with enough variety to catch position and row errors."""
    return [[((x // 3) + (y // 2) * 3 + seed) % 7 + 1 if (x + y + seed) % 11 else BLACK for x in range(w)] for y in range(h)]


def cases():
    W, H = 40, 30
    yield "full-frames", gif(W, H, [{"rows": pattern(W, H, s), "delay": 10 + s} for s in range(4)]), {}
    yield "partial-keep", gif(W, H, [{"rows": pattern(W, H)}, {"colour": RED, "x": 5, "y": 4, "w": 10, "h": 8},
                                     {"colour": GREEN, "x": 25, "y": 18, "w": 15, "h": 12}]), {}
    yield "disposal-background", gif(W, H, [{"rows": pattern(W, H)}, {"colour": RED, "x": 5, "y": 4, "w": 10, "h": 8, "disposal": 2},
                                            {"colour": GREEN, "x": 8, "y": 6, "w": 10, "h": 8}, {"colour": BLUE, "x": 30, "y": 0, "w": 5, "h": 5}]), {}
    yield "disposal-previous", gif(W, H, [{"rows": pattern(W, H)}, {"colour": RED, "x": 5, "y": 4, "w": 10, "h": 8, "disposal": 3},
                                          {"colour": GREEN, "x": 8, "y": 6, "w": 10, "h": 8, "disposal": 3},
                                          {"colour": BLUE, "x": 30, "y": 0, "w": 5, "h": 5}]), {}
    holes = [[WHITE if x % 2 else CYAN for x in range(20)] for _ in range(10)]
    yield "transparency", gif(W, H, [{"rows": pattern(W, H)}, {"rows": holes, "x": 4, "y": 4, "w": 20, "h": 10, "transparent": WHITE},
                                     {"colour": PINK, "x": 0, "y": 0, "w": 6, "h": 6}]), {}
    # Transparency applies to one frame only: a later frame's WHITE must paint
    yield "transparency-resets", gif(W, H, [{"colour": RED}, {"colour": WHITE, "x": 2, "y": 2, "w": 8, "h": 8, "transparent": WHITE},
                                            {"colour": WHITE, "x": 12, "y": 2, "w": 8, "h": 8}]), {}
    swapped = [(9, 9, 9), (10, 200, 30), (200, 30, 10), (30, 10, 200), (250, 250, 250), (120, 60, 0), (0, 120, 60), (60, 0, 120)]
    yield "local-palette", gif(W, H, [{"rows": pattern(W, H)}, {"rows": pattern(W, H, 3), "palette": swapped}, {"rows": pattern(W, H, 5)}]), {}
    yield "interlaced", gif(W, H, [{"rows": pattern(W, H), "interlaced": True}, {"rows": pattern(16, 13, 2), "x": 3, "y": 5, "w": 16, "h": 13, "interlaced": True}]), {}
    yield "delays", gif(W, H, [{"colour": RED, "delay": 0}, {"colour": GREEN, "delay": 1}, {"colour": BLUE, "delay": 2}, {"colour": WHITE, "delay": 150}]), \
        {"delays": [100, 100, 20, 1500]}
    yield "plays-once", gif(W, H, [{"colour": RED}, {"colour": GREEN}], loops=None), {"plays": 1}
    yield "plays-twice", gif(W, H, [{"colour": RED}, {"colour": GREEN}], loops=1), {"plays": 2}
    yield "loops-forever", gif(W, H, [{"colour": RED}, {"colour": GREEN, "x": 2, "y": 2, "w": 5, "h": 5}, {"colour": BLUE, "x": 9, "y": 9, "w": 5, "h": 5}]), {"forever": True}
    # The first frame covers only part of the picture: looping must clear what later frames painted
    yield "partial-first-frame", gif(W, H, [{"colour": RED, "x": 5, "y": 5, "w": 8, "h": 8}, {"colour": GREEN, "x": 20, "y": 15, "w": 8, "h": 8}]), {"forever": True}
    # Long LZW data: table resets and code growth across many pixels
    W2, H2 = 200, 150
    noisy = [[(x * x + y * 7 + x * y) % 8 for x in range(W2)] for y in range(H2)]
    yield "big-table", gif(W2, H2, [{"rows": noisy}, {"rows": [r[::-1] for r in noisy]}]), {}


def rgb565(rgba_image):
    """Reduces to RGB565 over black, as little-endian bytes."""
    px = rgba_image.convert("RGBA").tobytes()
    out = bytearray()
    for i in range(0, len(px), 4):
        r, g, b, a = px[i:i + 4]
        if a == 0:
            r = g = b = 0
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out += struct.pack("<H", v)
    return bytes(out)


def pillow_frames(path):
    im = Image.open(path)
    frames = []
    for i in range(im.n_frames):
        im.seek(i)
        frames.append(rgb565(im))
    return frames


class Checks:
    def __init__(self, tool, tmp):
        self.tool, self.tmp, self.failures = tool, tmp, 0

    def check(self, ok, msg):
        if not ok:
            self.failures += 1
            print("FAIL", msg)

    def play(self, path, max_pixels=1000000, limit=500):
        raw = self.tmp / "out.raw"
        out = subprocess.run([str(self.tool), str(path), str(raw), str(max_pixels), str(limit)], capture_output=True, text=True, check=True).stdout
        lines = out.splitlines()
        scan = [int(v) for v in lines[0].split()[1:]]
        began = lines[1].split()[1] == "1"
        delays = [int(l.split()[2]) for l in lines[2:]]
        indices = [int(l.split()[1]) for l in lines[2:]]
        pics = []
        if began and delays:
            data = raw.read_bytes()
            size = scan[2] * scan[3] * 2
            pics = [data[i * size:(i + 1) * size] for i in range(len(delays))]
        return scan, began, delays, indices, pics

    def run_case(self, name, data, opts):
        path = self.tmp / f"{name}.gif"
        path.write_bytes(data)
        ref = pillow_frames(path)
        # Files loop forever unless the case says otherwise: stop after one round, or three with "forever"
        if "plays" in opts:
            limit, expected = 500, len(ref) * opts["plays"]  # playback has to end by itself
        else:
            limit = expected = len(ref) * (3 if opts.get("forever") else 1)
        scan, began, delays, indices, pics = self.play(path, limit=limit)
        self.check(began and scan[0] and scan[1], f"{name}: not played")
        self.check(scan[4] == len(ref), f"{name}: {scan[4]} frames, Pillow has {len(ref)}")
        self.check(len(pics) == expected, f"{name}: played {len(pics)} pictures, want {expected}")
        for i, pic in enumerate(pics):
            want = ref[i % len(ref)]
            if pic != want:
                wrong = sum(pic[j:j + 2] != want[j:j + 2] for j in range(0, len(want), 2))
                self.check(False, f"{name}: picture {i} differs from Pillow in {wrong} pixels")
                break
            self.check(indices[i] == i % len(ref), f"{name}: picture {i} reports frame {indices[i]}")
        if "delays" in opts:
            self.check(delays == opts["delays"], f"{name}: delays {delays}, want {opts['delays']}")

    def run_limits(self):
        anim = gif(40, 30, [{"colour": RED}, {"colour": GREEN}, {"colour": BLUE}])
        p = self.tmp / "limits.gif"
        p.write_bytes(anim)
        # Cut off like the 300 KB download limit, anywhere: never played
        for cut in (len(anim) - 1, len(anim) // 2, 40, 13):
            p.write_bytes(anim[:cut])
            _, began, *_ = self.play(p)
            self.check(not began, f"file cut at {cut} of {len(anim)} bytes was played")
        p.write_bytes(anim)
        _, began, *_ = self.play(p, max_pixels=1199)
        self.check(not began, "too many pixels but played")
        _, began, *_ = self.play(p, max_pixels=1200)
        self.check(began, "exactly at the limit but not played")
        one = gif(40, 30, [{"colour": RED}])
        p.write_bytes(one)
        _, began, *_ = self.play(p)
        self.check(not began, "single-frame file played")
        # Damaged bytes must not crash, whatever they do to the picture
        damaged = bytearray(anim)
        for i in range(30, len(damaged)):
            b = bytearray(damaged)
            b[i] ^= 0xFF
            p.write_bytes(bytes(b))
            self.play(p, limit=20)


def main():
    keep = Path(sys.argv[1]) if len(sys.argv) > 1 else None
    tmp = Path(tempfile.mkdtemp(prefix="giftest"))
    try:
        tool = tmp / "gif_dump"
        subprocess.run(["c++", "-std=c++17", "-O1", "-g", "-Wall", "-fsanitize=address,undefined", str(HERE / "gif_dump.cpp"), "-o", str(tool)], check=True)
        checks = Checks(tool, tmp)
        for name, data, opts in cases():
            checks.run_case(name, data, opts)
            if keep:
                keep.mkdir(parents=True, exist_ok=True)
                (keep / f"{name}.gif").write_bytes(data)
        checks.run_limits()
        if checks.failures:
            print(f"{checks.failures} check(s) failed")
            return 1
        print("All GIF tests passed")
        return 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
