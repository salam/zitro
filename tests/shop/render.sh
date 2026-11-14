#!/bin/bash
# Draws the shop game's screens with the stick's drawing code and saves them as PNG (3x, like the stick's photos)
# plus one overview: tests/shop/render.sh [output folder, default screenshots/shop]
set -e
cd "$(dirname "$0")"
OUTDIR="${1:-../../screenshots/shop}"
mkdir -p "$OUTDIR"
WORK="${TMPDIR:-/tmp}/zitro_shop_render_$$"
mkdir -p "$WORK"
c++ -std=c++17 -O2 -Wall -Wextra render.cpp -o "$WORK/render"
"$WORK/render" "$WORK" > /dev/null
python3 - "$WORK" "$OUTDIR" <<'EOF'
import glob, os, sys
from PIL import Image, ImageDraw
work, out = sys.argv[1], sys.argv[2]
files = sorted(glob.glob(os.path.join(work, "*.ppm")))
shots = []
for f in files:
    im = Image.open(f).convert("RGB")
    big = im.resize((im.width * 3, im.height * 3), Image.NEAREST)
    name = os.path.splitext(os.path.basename(f))[0]
    big.save(os.path.join(out, name + ".png"))
    shots.append((name, im))
cols, scale, pad, label = 3, 2, 16, 18
w, h = 240 * scale, 135 * scale
rows = (len(shots) + cols - 1) // cols
sheet = Image.new("RGB", (cols * (w + pad) + pad, rows * (h + pad + label) + pad), (24, 24, 28))
d = ImageDraw.Draw(sheet)
for k, (name, im) in enumerate(shots):
    x = pad + (k % cols) * (w + pad)
    y = pad + (k // cols) * (h + pad + label)
    d.text((x, y), name, fill=(220, 220, 220))
    sheet.paste(im.resize((w, h), Image.NEAREST), (x, y + label))
sheet.save(os.path.join(out, "overview.png"))
print(f"{len(shots)} screens -> {out}")
EOF
rm -rf "$WORK"
