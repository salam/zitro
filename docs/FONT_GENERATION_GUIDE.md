# Font Generation Guide

## Quick Start

The AI Stick now has a two-level font system that supports 30+ languages. **Custom fonts are optional** - the device will work with built-in fonts until you generate custom ones.

## Current Status

✅ **Font system implemented** - Code is ready and compiling
⏳ **Font files needed** - Generate using instructions below
📦 **Fallback active** - Uses built-in Font 2 until custom fonts are ready

## Generating Custom Fonts

### Tools Needed

Choose one of these font conversion tools:

1. **truetype2gfx** (Recommended - Web-based, no installation)
   - URL: https://rop.nl/truetype2gfx/
   - Easy to use, immediate results

2. **M5Stack FontCreator** (Windows GUI)
   - URL: https://github.com/m5stack/FontCreator
   - More features, larger file support

3. **Adafruit fontconvert** (Command-line)
   - Requires compilation, for advanced users

### Step 1: Download Base Font

Download a TrueType font with good Unicode coverage:

- **Recommended**: [Roboto Regular](https://fonts.google.com/specimen/Roboto) from Google Fonts
- **Alternative**: DejaVu Sans, Liberation Sans, or Noto Sans

### Step 2: Generate Primary Font (12pt)

Using truetype2gfx:

1. Go to https://rop.nl/truetype2gfx/
2. Click "Select font file" → Upload `Roboto-Regular.ttf`
3. Font size: **12**
4. Font name: `PrimaryFont12pt`
5. Enter character ranges (in hexadecimal, one per line):
   ```
   0x20-0x7F
   0x80-0xFF
   0x100-0x17F
   0x370-0x3FF
   0x400-0x4FF
   0x590-0x5FF
   0x2013-0x2026
   ```
6. Click "Create" button
7. Download the generated `.h` file
8. Save as `sketch/sketch_jan12a/fonts/PrimaryFont12pt.h`

**Character ranges explained:**
- `0x20-0x7F`: Basic Latin (English, numbers, punctuation)
- `0x80-0xFF`: Latin-1 Supplement (German ä,ö,ü,ß, French, Spanish)
- `0x100-0x17F`: Latin Extended-A (Polish, Czech, Turkish)
- `0x370-0x3FF`: Greek and Coptic
- `0x400-0x4FF`: Cyrillic (Russian, Ukrainian, Bulgarian)
- `0x590-0x5FF`: Hebrew
- `0x2013-0x2026`: General Punctuation (em-dash, ellipsis, smart quotes)

### Step 3: Generate Primary Font (16pt)

Repeat Step 2 with these changes:
- Font size: **16**
- Font name: `PrimaryFont16pt`
- Save as `sketch/sketch_jan12a/fonts/PrimaryFont16pt.h`

### Step 4: Verify Installation

1. Recompile the sketch: `./verify.sh`
2. Look for this message in serial output:
   ```
   FontManager: Using custom primary fonts
   ```
3. If you see "Custom fonts not found", check file locations

### Step 5: Upload and Test

1. Upload sketch to M5StickC Plus 2
2. Test with German text: "Hällo Wörld! Schön dass du da bist."
3. Characters should render correctly without conversion

## Optional: CJK Font Support

⚠️ **Only if you need Chinese/Japanese/Korean support**

CJK fonts are large (~1.4 MB total) and only needed if you're displaying Asian text.

### Enable CJK Support

1. Edit `sketch/sketch_jan12a/sketch_jan12a.ino`
2. Find line ~28: `// #define ENABLE_CJK_FONTS`
3. Uncomment it: `#define ENABLE_CJK_FONTS`

### Generate CJK Fonts

**Requirements:**
- Noto Sans CJK font (download from Google Fonts)
- More powerful conversion tool (M5Stack FontCreator recommended)

**Warning**: Web-based truetype2gfx may timeout on such large character sets.

1. Download [Noto Sans CJK Regular](https://www.google.com/get/noto/)
2. Use M5Stack FontCreator (Windows) or fontconvert
3. Font size: **12** → Generate `ExtendedFont12pt.h`
4. Font size: **16** → Generate `ExtendedFont16pt.h`
5. Character range: `0x4E00-0x9FFF` (20,992 characters)
6. Save both files to `sketch/sketch_jan12a/fonts/`

Expected file sizes:
- ExtendedFont12pt.h: ~700 KB
- ExtendedFont16pt.h: ~800 KB

## Troubleshooting

### Fonts don't render correctly

**Problem**: Characters show as boxes or incorrectly
**Solution**:
- Verify font files are in `sketch/sketch_jan12a/fonts/` directory
- Check serial output for "FontManager: Using custom primary fonts"
- Ensure character ranges were entered correctly

### Compilation fails

**Problem**: "file not found" or "undeclared identifier"
**Solution**:
- Font file names must match exactly: `PrimaryFont12pt.h` and `PrimaryFont16pt.h`
- Check that files are in the correct directory
- Recompile: `./verify.sh`

### Web tool times out

**Problem**: truetype2gfx times out with large character sets
**Solution**:
- Reduce character range (split into smaller fonts)
- Use M5Stack FontCreator desktop app instead
- Use fontconvert command-line tool

### File too large error

**Problem**: Generated font file is too big
**Solution**:
- Reduce font size (use 10pt instead of 12pt)
- Remove unused character ranges
- Use font compression (M5Stack FontCreator option)

## Memory Usage

| Configuration | Flash Used | Remaining | Status |
|--------------|-----------|-----------|--------|
| No custom fonts (built-in) | 1.55 MB | 6.45 MB | ✅ Current |
| Primary fonts only | 1.60 MB | 6.40 MB | ✅ Recommended |
| Primary + CJK fonts | 3.00 MB | 5.00 MB | ⚠️ Optional |

Total flash available: 8 MB

## Testing

### Test European Languages

Try these phrases after uploading:

- **German**: "Hällo Wörld! Schöne Grüße aus München."
- **French**: "Bonjour! Ça va très bien aujourd'hui."
- **Spanish**: "¡Hola! ¿Cómo estás?"
- **Russian**: "Привет мир! Как дела?"
- **Greek**: "Γεια σου κόσμε!"
- **Hebrew**: "שלום עולם"

All should display correctly without character substitution.

### Test CJK (If Enabled)

- **Chinese**: "你好世界"
- **Japanese**: "こんにちは世界"
- **Korean**: "안녕하세요 세계"

## Font Customization

### Using Different Base Fonts

You can use any TrueType font with Unicode coverage:

- **For readability**: Roboto, Open Sans, Source Sans Pro
- **For space efficiency**: Noto Sans, Liberation Sans
- **For style**: Lato, Raleway, Montserrat

### Adjusting Font Sizes

Current sizes (12pt, 16pt) were chosen to match existing UI. To change:

1. Generate fonts at desired size (e.g., 14pt, 18pt)
2. Update threshold in code:
   ```cpp
   // In drawTextSmart function (line ~176)
   bool largeFont = (textSize >= 1.75); // Adjust this threshold
   ```

### Creating Additional Font Variants

For bold, italic, or different weights:

1. Download font variant (e.g., Roboto-Bold.ttf)
2. Generate with same character ranges
3. Name appropriately: `PrimaryFontBold12pt.h`
4. Modify `FontManager` class to support variants

## Advanced: Subset Fonts

To save space, create subsets for specific languages:

### German-Only Subset

Character ranges:
```
0x20-0x7F    (Basic Latin)
0xC0-0xC6    (À-Æ)
0xC8-0xCF    (È-Ï)
0xD1-0xD6    (Ñ-Ö)
0xD9-0xDC    (Ù-Ü)
0xDF-0xE6    (ß-æ)
0xE8-0xEF    (è-ï)
0xF1-0xF6    (ñ-ö)
0xF9-0xFC    (ù-ü)
```

Result: ~120 characters instead of 820 → ~15% file size

## Next Steps

After generating fonts:

1. ✅ Compile sketch successfully
2. ✅ Upload to device
3. ✅ Test with multilingual text
4. 🎉 Enjoy native umlaut rendering!

## Support

- Font conversion issues → Check [truetype2gfx documentation](https://rop.nl/truetype2gfx/)
- M5Stack FontCreator → See [GitHub repo](https://github.com/m5stack/FontCreator)
- System architecture → Read `docs/FONT_SYSTEM_ARCHITECTURE.md`
- Implementation details → Read `docs/FONT_IMPLEMENTATION_PLAN.md`
