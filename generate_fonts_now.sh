#!/bin/bash
# Quick Font Generation - Uses existing Roboto font
set -e

FONT_FILE="fonts/Roboto-VariableFont_wdth,wght.ttf"
FONTS_DIR="sketch/sketch_jan12a/fonts"

echo "======================================"
echo "  Generating AI Stick Fonts"
echo "======================================"

# Compile fontconvert if needed
if [ ! -f "/tmp/fontconvert" ]; then
    echo "Setting up fontconvert..."
    cd /tmp
    curl -sL "https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/master/gfxfont.h" -o gfxfont.h
    curl -sL "https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/master/fontconvert/fontconvert.c" -o fontconvert.c
    sed 's|../gfxfont.h|gfxfont.h|' fontconvert.c > fontconvert_fixed.c
    gcc -o fontconvert fontconvert_fixed.c $(freetype-config --cflags --libs)
    cd - > /dev/null
    echo "✓ fontconvert ready"
fi

mkdir -p "$FONTS_DIR"

# Generate fonts with full Unicode ranges
echo ""
echo "Generating PrimaryFont12pt.h..."
/tmp/fontconvert "$FONT_FILE" 12 0x20 0x5FF > "$FONTS_DIR/PrimaryFont12pt.h"
echo "✓ Created ($(du -h "$FONTS_DIR/PrimaryFont12pt.h" | cut -f1))"

echo ""
echo "Generating PrimaryFont16pt.h..."
/tmp/fontconvert "$FONT_FILE" 16 0x20 0x5FF > "$FONTS_DIR/PrimaryFont16pt.h"
echo "✓ Created ($(du -h "$FONTS_DIR/PrimaryFont16pt.h" | cut -f1))"

echo ""
echo "======================================"
echo "✅ Fonts generated successfully!"
echo "======================================"
echo ""
echo "Files created:"
ls -lh "$FONTS_DIR"/*.h
echo ""
echo "Compiling..."
./verify.sh
