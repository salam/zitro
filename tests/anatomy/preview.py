#!/usr/bin/env python3
"""Contact sheet of the real files, drawn by the device pipeline at 240x135 and enlarged, to check by eye.
Usage: preview.py <render_bin> <data dir> <out.png> <id> [<id> ...]"""
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

VIEWS = [("front", 0.0, 0.0, 0), ("start", 0.35, 0.5, 0), ("side", 0.0, 1.57, 0), ("back", 0.0, 3.14, 0), ("zoom", 0.35, 0.5, 2)]


def main():
    binary, data, out, ids = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
    scale = 2
    sheet = Image.new("RGB", ((240 * scale + 6) * len(VIEWS), (135 * scale + 18) * len(ids)), (40, 40, 40))
    draw = ImageDraw.Draw(sheet)
    with tempfile.TemporaryDirectory() as tmp:
        for row, model_id in enumerate(ids):
            for col, (name, rx, ry, zoom) in enumerate(VIEWS):
                ppm = os.path.join(tmp, "x.ppm")
                result = subprocess.run([binary, os.path.join(data, model_id + ".bin"), model_id, ppm, str(rx), str(ry), str(zoom)],
                                        capture_output=True, text=True)
                x, y = col * (240 * scale + 6), row * (135 * scale + 18)
                if result.returncode:
                    draw.text((x + 4, y + 4), f"{model_id}: {result.stderr}", fill=(255, 80, 80))
                    continue
                image = Image.open(ppm).resize((240 * scale, 135 * scale), Image.NEAREST)
                sheet.paste(image, (x, y + 16))
                draw.text((x + 4, y + 2), f"{model_id} {name} {result.stdout.strip().split(' ', 1)[1]}", fill=(255, 255, 255))
    sheet.save(out)


if __name__ == "__main__":
    main()
