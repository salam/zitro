#!/bin/bash
# Font Generation Helper Script
# This script guides you through generating custom fonts for the AI Stick

set -e

FONTS_DIR="sketch/sketch_jan12a/fonts"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "======================================"
echo "  AI Stick Font Generation Helper"
echo "======================================"
echo ""

# Check if fonts directory exists
if [ ! -d "$FONTS_DIR" ]; then
    echo "Error: Fonts directory not found: $FONTS_DIR"
    exit 1
fi

echo "Font generation requires external tools. Choose your method:"
echo ""
echo "1. Web-based: truetype2gfx (easiest, recommended)"
echo "2. Desktop: M5Stack FontCreator (Windows only)"
echo "3. Command-line: Adafruit fontconvert (advanced)"
echo ""
read -p "Select option (1-3): " choice

case $choice in
    1)
        echo ""
        echo "========================================"
        echo "Web-based Font Generation (truetype2gfx)"
        echo "========================================"
        echo ""
        echo "Step 1: Download Roboto Font"
        echo "  → Go to: https://fonts.google.com/specimen/Roboto"
        echo "  → Click 'Download family'"
        echo "  → Extract Roboto-Regular.ttf"
        echo ""
        echo "Step 2: Generate PrimaryFont12pt.h"
        echo "  → Go to: https://rop.nl/truetype2gfx/"
        echo "  → Upload: Roboto-Regular.ttf"
        echo "  → Font size: 12"
        echo "  → Font name: PrimaryFont12pt"
        echo "  → Character ranges (hex, one per line):"
        echo "      0x20-0x7F"
        echo "      0x80-0xFF"
        echo "      0x100-0x17F"
        echo "      0x370-0x3FF"
        echo "      0x400-0x4FF"
        echo "      0x590-0x5FF"
        echo "      0x2013-0x2026"
        echo "  → Click 'Create'"
        echo "  → Download and save to: $FONTS_DIR/PrimaryFont12pt.h"
        echo ""
        echo "Step 3: Generate PrimaryFont16pt.h"
        echo "  → Repeat Step 2 with:"
        echo "      Font size: 16"
        echo "      Font name: PrimaryFont16pt"
        echo "  → Save to: $FONTS_DIR/PrimaryFont16pt.h"
        echo ""
        read -p "Press Enter when font files are saved..."
        ;;
    2)
        echo ""
        echo "========================================"
        echo "Desktop Font Generation (M5Stack FontCreator)"
        echo "========================================"
        echo ""
        echo "1. Download M5Stack FontCreator:"
        echo "   → https://github.com/m5stack/FontCreator/releases"
        echo ""
        echo "2. Download Roboto Font:"
        echo "   → https://fonts.google.com/specimen/Roboto"
        echo ""
        echo "3. Use FontCreator GUI to generate fonts"
        echo "   → See: docs/FONT_GENERATION_GUIDE.md for details"
        echo ""
        read -p "Press Enter when font files are saved..."
        ;;
    3)
        echo ""
        echo "========================================"
        echo "Command-line Font Generation (fontconvert)"
        echo "========================================"
        echo ""
        echo "Installing Adafruit fontconvert..."

        # Check if we can compile fontconvert
        if ! command -v gcc &> /dev/null; then
            echo "Error: gcc not found. Install Xcode Command Line Tools:"
            echo "  xcode-select --install"
            exit 1
        fi

        echo "Downloading fontconvert source..."
        TEMP_DIR=$(mktemp -d)
        cd "$TEMP_DIR"

        curl -sL "https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/master/fontconvert/fontconvert.c" -o fontconvert.c

        echo "Compiling fontconvert..."
        gcc -o fontconvert fontconvert.c -lfreetype -I/usr/local/include/freetype2 -L/usr/local/lib 2>/dev/null || \
        gcc -o fontconvert fontconvert.c -lfreetype -I/opt/homebrew/include/freetype2 -L/opt/homebrew/lib 2>/dev/null || \
        (echo "Error: Could not compile. Install freetype: brew install freetype" && exit 1)

        sudo cp fontconvert /usr/local/bin/
        echo "✓ fontconvert installed"

        cd "$SCRIPT_DIR"
        rm -rf "$TEMP_DIR"

        echo ""
        echo "This method requires manual font generation."
        echo "See: docs/FONT_GENERATION_GUIDE.md"
        exit 0
        ;;
    *)
        echo "Invalid option"
        exit 1
        ;;
esac

# Verify font files exist
echo ""
echo "Verifying font files..."

if [ -f "$FONTS_DIR/PrimaryFont12pt.h" ]; then
    SIZE_12=$(wc -c < "$FONTS_DIR/PrimaryFont12pt.h")
    echo "✓ PrimaryFont12pt.h found ($(numfmt --to=iec-i --suffix=B $SIZE_12))"
else
    echo "✗ PrimaryFont12pt.h NOT FOUND"
    echo "  Expected location: $FONTS_DIR/PrimaryFont12pt.h"
    exit 1
fi

if [ -f "$FONTS_DIR/PrimaryFont16pt.h" ]; then
    SIZE_16=$(wc -c < "$FONTS_DIR/PrimaryFont16pt.h")
    echo "✓ PrimaryFont16pt.h found ($(numfmt --to=iec-i --suffix=B $SIZE_16))"
else
    echo "✗ PrimaryFont16pt.h NOT FOUND"
    echo "  Expected location: $FONTS_DIR/PrimaryFont16pt.h"
    exit 1
fi

echo ""
echo "======================================"
echo "Compiling sketch..."
echo "======================================"

./verify.sh

if [ $? -eq 0 ]; then
    echo ""
    echo "======================================"
    echo "✅ Success! Fonts are ready!"
    echo "======================================"
    echo ""
    echo "Next steps:"
    echo "1. Upload sketch to device"
    echo "2. Check serial output for:"
    echo "   'FontManager: Using custom primary fonts'"
    echo "3. Test with German text: Hällo Wörld!"
    echo ""
else
    echo ""
    echo "❌ Compilation failed. Check errors above."
    exit 1
fi
