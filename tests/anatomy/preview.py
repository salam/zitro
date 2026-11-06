#!/usr/bin/env python3
"""Contact sheet of the real files, drawn by the device pipeline at 240x135 and enlarged, to check by eye.
Usage: preview.py <render_bin> <data dir> <out.png> <id> [<id> ...]
An id like heart-cut, or a cell, is drawn as a cutaway: at its start pose and at the limits the hand can turn it to,
with the legend roughly where the stick puts it (the font here only stands in for the stick's)."""
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont

VIEWS = [("front", 0.0, 0.0, 0), ("start", 0.35, 0.5, 0), ("side", 0.0, 1.57, 0), ("back", 0.0, 3.14, 0), ("zoom", 0.35, 0.5, 2)]
CUT_VIEWS = [("start", 0.0, 0.0, 0), ("left", 0.0, -1.3, 0), ("right", 0.0, 0.7, 0), ("up", -0.9, 0.0, 0), ("zoom", 0.0, 0.0, 2)]
CELLS = ("animal-cell", "plant-cell", "red-blood-cell", "neuron")
SCALE = 2


def rgb(p):
    return ((p >> 11) << 3, ((p >> 5) & 0x3F) << 2, (p & 0x1F) << 3)


def legend(image, top, schematic, lines, language):
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default(size=11)
    if schematic:
        draw.rectangle((2, 2, 118, 12), fill=(0xF2, 0xB7, 0x05))
        draw.text((5, 1), "Vereinfachtes Schema", font=ImageFont.load_default(size=9), fill=(0, 0, 0))
    for i, (color, words) in enumerate(lines):
        y = top + i * 14
        if color >= 0:
            draw.rectangle((112, y + 1, 120, y + 9), fill=rgb(color))
        draw.text((124 if color >= 0 else 112, y - 1), words.split("|")[language], font=font, fill=(255, 255, 255) if color >= 0 else (170, 170, 170))


def main():
    binary, data, out, ids = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
    sheet = Image.new("RGB", ((240 * SCALE + 6) * len(VIEWS), (135 * SCALE + 18) * len(ids)), (40, 40, 40))
    draw = ImageDraw.Draw(sheet)
    with tempfile.TemporaryDirectory() as tmp:
        for row, name in enumerate(ids):
            model_id = name[:-4] if name.endswith("-cut") else name
            cut = name.endswith("-cut") or name in CELLS
            for col, (view, rx, ry, zoom) in enumerate(CUT_VIEWS if cut else VIEWS):
                ppm = os.path.join(tmp, "x.ppm")
                result = subprocess.run([binary, os.path.join(data, name + ".bin"), model_id, ppm, str(rx), str(ry), str(zoom)] + (["cut"] if cut else []),
                                        capture_output=True, text=True)
                x, y = col * (240 * SCALE + 6), row * (135 * SCALE + 18)
                if result.returncode:
                    draw.text((x + 4, y + 4), f"{name}: {result.stderr}", fill=(255, 80, 80))
                    continue
                lines = result.stdout.strip().split("\n")
                image = Image.open(ppm).convert("RGB")
                entries = [(int(l.split(" ", 2)[1]), l.split(" ", 2)[2]) for l in lines if l.startswith("legend ")]
                top = [l.split() for l in lines if l.startswith("top ")]
                if entries:
                    legend(image, int(top[0][1]), top[0][2] == "1", entries, col % 2)
                sheet.paste(image.resize((240 * SCALE, 135 * SCALE), Image.NEAREST), (x, y + 16))
                draw.text((x + 4, y + 2), f"{name} {view} {lines[0].split(' ', 1)[1]}", fill=(255, 255, 255))
    sheet.save(out)


if __name__ == "__main__":
    main()
