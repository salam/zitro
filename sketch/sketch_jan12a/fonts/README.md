# Font Files Directory

This directory contains custom font files for the two-level font system.

## Current Status

⚠️ **Font files need to be generated** using the instructions in `/docs/FONT_IMPLEMENTATION_PLAN.md`

## Required Font Files

### Level 1: Primary Fonts (Required)
- `PrimaryFont12pt.h` - European/Greek/Cyrillic/Hebrew characters at 12pt
- `PrimaryFont16pt.h` - European/Greek/Cyrillic/Hebrew characters at 16pt

### Level 2: Extended Fonts (Optional)
- `ExtendedFont12pt.h` - CJK characters at 12pt (only if ENABLE_CJK_FONTS defined)
- `ExtendedFont16pt.h` - CJK characters at 16pt (only if ENABLE_CJK_FONTS defined)

## How to Generate Font Files

Use the [truetype2gfx online tool](https://rop.nl/truetype2gfx/) to generate custom fonts.

### Step 1: Generate PrimaryFont12pt.h

1. Go to https://rop.nl/truetype2gfx/
2. Upload a TrueType font (recommended: Roboto-Regular.ttf from Google Fonts)
3. Font size: **12**
4. Character ranges (hex):
   ```
   0x20-0x7F
   0x80-0xFF
   0x100-0x17F
   0x370-0x3FF
   0x400-0x4FF
   0x590-0x5FF
   0x2013-0x2026
   ```
5. Font name: `PrimaryFont12pt`
6. Download and save as `PrimaryFont12pt.h`

### Step 2: Generate PrimaryFont16pt.h

Repeat above with font size **16** and name `PrimaryFont16pt`

### Step 3: Generate CJK Fonts (Optional)

For CJK support, generate ExtendedFont12pt.h and ExtendedFont16pt.h with:
- Character range: `0x4E00-0x9FFF`
- Use Noto Sans CJK font

## Temporary Fallback

Until custom fonts are generated, the system uses M5GFX built-in Font 2 as a fallback.
Characters outside ASCII range may not display correctly.

## Expected File Sizes

- PrimaryFont12pt.h: ~20 KB
- PrimaryFont16pt.h: ~30 KB
- ExtendedFont12pt.h: ~700 KB (optional)
- ExtendedFont16pt.h: ~800 KB (optional)
