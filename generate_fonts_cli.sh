#!/bin/bash
# Automated Font Generation Using Command-Line Tools
# Generates custom fonts for AI Stick with full Unicode support

set -e

FONTS_DIR="sketch/sketch_jan12a/fonts"
TEMP_DIR="/tmp/ai-stick-fonts-$$"

echo "======================================"
echo "  AI Stick Font Generator (CLI)"
echo "======================================"
echo ""

# Create temp directory
mkdir -p "$TEMP_DIR"
cd "$TEMP_DIR"

# Step 1: Compile fontconvert if not exists
if [ ! -f "/tmp/fontconvert" ]; then
    echo "Step 1/4: Compiling fontconvert tool..."
    curl -sL "https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/master/gfxfont.h" -o gfxfont.h
    curl -sL "https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/master/fontconvert/fontconvert.c" -o fontconvert.c
    sed 's|../gfxfont.h|gfxfont.h|' fontconvert.c > fontconvert_fixed.c
    gcc -o /tmp/fontconvert fontconvert_fixed.c $(freetype-config --cflags --libs)
    echo "✓ fontconvert compiled"
else
    echo "Step 1/4: Using existing fontconvert tool"
fi

# Step 2: Download Roboto font
echo ""
echo "Step 2/4: Downloading Roboto font..."
if [ ! -f "$TEMP_DIR/Roboto-Regular.ttf" ]; then
    # Try to find Roboto on system first
    SYSTEM_ROBOTO=$(find /System/Library/Fonts /Library/Fonts ~/Library/Fonts -name "Roboto-Regular.ttf" 2>/dev/null | head -1)

    if [ -n "$SYSTEM_ROBOTO" ]; then
        echo "✓ Found system Roboto: $SYSTEM_ROBOTO"
        cp "$SYSTEM_ROBOTO" "$TEMP_DIR/Roboto-Regular.ttf"
    else
        # Use a good system font as fallback
        echo "Roboto not found, using Verdana as fallback..."
        FALLBACK_FONT="/System/Library/Fonts/Supplemental/Verdana.ttf"
        if [ -f "$FALLBACK_FONT" ]; then
            cp "$FALLBACK_FONT" "$TEMP_DIR/Roboto-Regular.ttf"
            echo "✓ Using Verdana font"
        else
            echo "Error: No suitable font found"
            echo "Please download Roboto from: https://fonts.google.com/specimen/Roboto"
            exit 1
        fi
    fi
else
    echo "✓ Font file already exists"
fi

# Step 3: Generate font header files
echo ""
echo "Step 3/4: Generating font files..."

# Character ranges for primary font
# We need to generate multiple ranges and combine them

# Function to generate font for a range
generate_range() {
    local SIZE=$1
    local START=$2
    local END=$3
    local OUTPUT=$4

    /tmp/fontconvert "$TEMP_DIR/Roboto-Regular.ttf" $SIZE $START $END >> "$OUTPUT" 2>/dev/null
}

# Generate 12pt font
echo "  → Generating PrimaryFont12pt.h..."
OUTPUT_12="$TEMP_DIR/PrimaryFont12pt_temp.h"
> "$OUTPUT_12"  # Clear file

# Generate each range
generate_range 12 0x20 0x7F "$OUTPUT_12"     # Basic Latin
generate_range 12 0x80 0xFF "$OUTPUT_12"     # Latin-1 Supplement
generate_range 12 0x100 0x17F "$OUTPUT_12"   # Latin Extended-A
generate_range 12 0x370 0x3FF "$OUTPUT_12"   # Greek
generate_range 12 0x400 0x4FF "$OUTPUT_12"   # Cyrillic
generate_range 12 0x590 0x5FF "$OUTPUT_12"   # Hebrew
generate_range 12 0x2013 0x2026 "$OUTPUT_12" # Punctuation

# Add proper header and wrapper
cat > "$TEMP_DIR/PrimaryFont12pt.h" << 'EOF'
// PrimaryFont12pt.h - Custom font for AI Stick
// Generated using Adafruit fontconvert
// European languages: Latin, Greek, Cyrillic, Hebrew support

#ifndef _PRIMARYFONT12PT_H_
#define _PRIMARYFONT12PT_H_

#include <Arduino.h>

EOF

# Extract font data from temp file and clean it up
grep -v "^//" "$OUTPUT_12" | grep -v "^$" >> "$TEMP_DIR/PrimaryFont12pt.h"

cat >> "$TEMP_DIR/PrimaryFont12pt.h" << 'EOF'

#endif // _PRIMARYFONT12PT_H_
EOF

echo "✓ PrimaryFont12pt.h created ($(wc -c < "$TEMP_DIR/PrimaryFont12pt.h" | tr -d ' ') bytes)"

# Generate 16pt font
echo "  → Generating PrimaryFont16pt.h..."
OUTPUT_16="$TEMP_DIR/PrimaryFont16pt_temp.h"
> "$OUTPUT_16"  # Clear file

# Generate each range for 16pt
generate_range 16 0x20 0x7F "$OUTPUT_16"
generate_range 16 0x80 0xFF "$OUTPUT_16"
generate_range 16 0x100 0x17F "$OUTPUT_16"
generate_range 16 0x370 0x3FF "$OUTPUT_16"
generate_range 16 0x400 0x4FF "$OUTPUT_16"
generate_range 16 0x590 0x5FF "$OUTPUT_16"
generate_range 16 0x2013 0x2026 "$OUTPUT_16"

# Add proper header and wrapper
cat > "$TEMP_DIR/PrimaryFont16pt.h" << 'EOF'
// PrimaryFont16pt.h - Custom font for AI Stick
// Generated using Adafruit fontconvert
// European languages: Latin, Greek, Cyrillic, Hebrew support

#ifndef _PRIMARYFONT16PT_H_
#define _PRIMARYFONT16PT_H_

#include <Arduino.h>

EOF

grep -v "^//" "$OUTPUT_16" | grep -v "^$" >> "$TEMP_DIR/PrimaryFont16pt.h"

cat >> "$TEMP_DIR/PrimaryFont16pt.h" << 'EOF'

#endif // _PRIMARYFONT16PT_H_
EOF

echo "✓ PrimaryFont16pt.h created ($(wc -c < "$TEMP_DIR/PrimaryFont16pt.h" | tr -d ' ') bytes)"

# Step 4: Install fonts
echo ""
echo "Step 4/4: Installing fonts..."
cd "$OLDPWD"

mkdir -p "$FONTS_DIR"
cp "$TEMP_DIR/PrimaryFont12pt.h" "$FONTS_DIR/"
cp "$TEMP_DIR/PrimaryFont16pt.h" "$FONTS_DIR/"

echo "✓ Fonts installed to $FONTS_DIR/"

# Cleanup
rm -rf "$TEMP_DIR"

echo ""
echo "======================================"
echo "Compiling sketch..."
echo "======================================"
echo ""

./verify.sh

if [ $? -eq 0 ]; then
    echo ""
    echo "======================================"
    echo "✅ Success! Fonts generated!"
    echo "======================================"
    echo ""
    echo "Font files created:"
    ls -lh "$FONTS_DIR"/*.h
    echo ""
    echo "Next steps:"
    echo "1. Upload sketch to device"
    echo "2. Check serial output for:"
    echo "   'FontManager: Using custom primary fonts'"
    echo "3. Test with: Hällo Wörld! Привет! Γεια!"
    echo ""
else
    echo ""
    echo "❌ Compilation failed. Check errors above."
    exit 1
fi
