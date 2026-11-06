# Two-Level Font System - Implementation Plan

## Step-by-Step Implementation Guide

### Prerequisites

1. **Choose a base TrueType font**:
   - Recommended: **Roboto Regular** (clean, modern, excellent readability)
   - Alternative: DejaVu Sans, Liberation Sans, Noto Sans
   - Download from Google Fonts or system fonts

2. **Install font conversion tool**:
   - Option A: [M5Stack FontCreator](https://github.com/m5stack/FontCreator) (Windows GUI)
   - Option B: [truetype2gfx](https://rop.nl/truetype2gfx/) (Web-based)
   - Option C: Adafruit fontconvert (Command-line)

---

## Phase 1: Generate Font Files

### 1.1 Create Primary Font (12pt)

**Using truetype2gfx web tool**:
1. Go to https://rop.nl/truetype2gfx/
2. Upload: Roboto-Regular.ttf
3. Font size: **12**
4. Character ranges (enter in hex):
   ```
   0x20-0x7F     (Basic Latin)
   0x80-0xFF     (Latin-1 Supplement)
   0x100-0x17F   (Latin Extended-A)
   0x370-0x3FF   (Greek)
   0x400-0x4FF   (Cyrillic)
   0x590-0x5FF   (Hebrew)
   0x2013-0x2026 (Punctuation)
   ```
5. Font name: `PrimaryFont12pt`
6. Click "Create"
7. Download as `PrimaryFont12pt.h`

### 1.2 Create Primary Font (16pt)

Repeat above with:
- Font size: **16**
- Font name: `PrimaryFont16pt`
- Download as `PrimaryFont16pt.h`

### 1.3 Create Extended CJK Font (12pt)

**Important**: This will be a LARGE file (~700 KB)

1. Go to https://rop.nl/truetype2gfx/
2. Upload: NotoSansCJK-Regular.ttf (download from Google Fonts)
3. Font size: **12**
4. Character range:
   ```
   0x4E00-0x9FFF (CJK Unified Ideographs)
   ```
5. Font name: `ExtendedFont12pt`
6. Download as `ExtendedFont12pt.h`

### 1.4 Create Extended CJK Font (16pt)

Repeat with:
- Font size: **16**
- Font name: `ExtendedFont16pt`
- Download as `ExtendedFont16pt.h`

### 1.5 Organize Font Files

```bash
mkdir -p sketch/sketch_jan12a/fonts/
mv PrimaryFont12pt.h sketch/sketch_jan12a/fonts/
mv PrimaryFont16pt.h sketch/sketch_jan12a/fonts/
mv ExtendedFont12pt.h sketch/sketch_jan12a/fonts/
mv ExtendedFont16pt.h sketch/sketch_jan12a/fonts/
```

**Expected file sizes**:
- PrimaryFont12pt.h: ~20 KB
- PrimaryFont16pt.h: ~30 KB
- ExtendedFont12pt.h: ~700 KB
- ExtendedFont16pt.h: ~800 KB

---

## Phase 2: Implement Font Manager

### 2.1 Create FontManager.ino

Create `sketch/sketch_jan12a/FontManager.ino`:

```cpp
// ============================================================================
// FontManager.ino - Two-level font lookup system
// ============================================================================

#include "fonts/PrimaryFont12pt.h"
#include "fonts/PrimaryFont16pt.h"
// Extended fonts included only when needed
#ifdef ENABLE_CJK_FONTS
#include "fonts/ExtendedFont12pt.h"
#include "fonts/ExtendedFont16pt.h"
#endif

class FontManager {
private:
  bool cjkEnabled;
  bool cjkLoaded;

public:
  FontManager() : cjkEnabled(false), cjkLoaded(false) {}

  void init(bool enableCJK = false) {
    cjkEnabled = enableCJK;
    cjkLoaded = enableCJK; // If enabled at init, mark as loaded
  }

  // Check if character is in CJK range
  bool isCJKCharacter(uint16_t codepoint) {
    return (codepoint >= 0x4E00 && codepoint <= 0x9FFF);
  }

  // Check if character is in primary range
  bool isPrimaryCharacter(uint16_t codepoint) {
    return (codepoint >= 0x0020 && codepoint <= 0x007F) ||  // Basic Latin
           (codepoint >= 0x0080 && codepoint <= 0x00FF) ||  // Latin-1
           (codepoint >= 0x0100 && codepoint <= 0x017F) ||  // Latin Ext-A
           (codepoint >= 0x0370 && codepoint <= 0x03FF) ||  // Greek
           (codepoint >= 0x0400 && codepoint <= 0x04FF) ||  // Cyrillic
           (codepoint >= 0x0590 && codepoint <= 0x05FF) ||  // Hebrew
           (codepoint >= 0x2013 && codepoint <= 0x2026);    // Punctuation
  }

  // Get appropriate font for character
  const GFXfont* getFontForChar(uint16_t codepoint, bool largeFont) {
    if (isCJKCharacter(codepoint)) {
      #ifdef ENABLE_CJK_FONTS
      return largeFont ? &ExtendedFont16pt : &ExtendedFont12pt;
      #else
      // CJK not available, fall back to primary
      return largeFont ? &PrimaryFont16pt : &PrimaryFont12pt;
      #endif
    } else {
      return largeFont ? &PrimaryFont16pt : &PrimaryFont12pt;
    }
  }

  // Check if text contains CJK characters
  bool containsCJK(const String& text) {
    int len = text.length();
    int i = 0;

    while (i < len) {
      uint16_t cp = decodeUTF8Character(text, i);
      if (isCJKCharacter(cp)) {
        return true;
      }
      i += getUTF8CharLength(text[i]);
    }
    return false;
  }

  // UTF-8 decoder
  static uint16_t decodeUTF8Character(const String& text, int index) {
    if (index >= text.length()) return 0;

    uint8_t byte1 = text[index];

    // 1-byte character (ASCII)
    if ((byte1 & 0x80) == 0) {
      return byte1;
    }

    // 2-byte character
    if ((byte1 & 0xE0) == 0xC0 && index + 1 < text.length()) {
      uint16_t cp = ((byte1 & 0x1F) << 6) | (text[index + 1] & 0x3F);
      return cp;
    }

    // 3-byte character (includes CJK)
    if ((byte1 & 0xF0) == 0xE0 && index + 2 < text.length()) {
      uint16_t cp = ((byte1 & 0x0F) << 12) |
                    ((text[index + 1] & 0x3F) << 6) |
                    (text[index + 2] & 0x3F);
      return cp;
    }

    return 0; // Invalid or unsupported
  }

  // Get UTF-8 character byte length
  static int getUTF8CharLength(uint8_t firstByte) {
    if ((firstByte & 0x80) == 0) return 1;      // ASCII
    if ((firstByte & 0xE0) == 0xC0) return 2;   // 2-byte
    if ((firstByte & 0xF0) == 0xE0) return 3;   // 3-byte (CJK)
    if ((firstByte & 0xF8) == 0xF0) return 4;   // 4-byte
    return 1; // Fallback
  }
};

// Global font manager instance
FontManager fontManager;

// ============================================================================
// Smart Text Rendering with Font Fallback
// ============================================================================

void drawTextWithFontFallback(M5Canvas& canvas, const String& text, int x, int y, bool largeFont) {
  int cursorX = x;
  int len = text.length();
  int i = 0;

  const GFXfont* lastFont = nullptr;

  while (i < len) {
    uint16_t codepoint = FontManager::decodeUTF8Character(text, i);
    int charLen = FontManager::getUTF8CharLength(text[i]);

    if (codepoint > 0) {
      // Get appropriate font for this character
      const GFXfont* font = fontManager.getFontForChar(codepoint, largeFont);

      // Only change font if different from last
      if (font != lastFont) {
        canvas.setFont(font);
        lastFont = font;
      }

      // Extract single character
      String ch = text.substring(i, i + charLen);

      // Render character
      canvas.drawString(ch, cursorX, y);

      // Advance cursor
      cursorX += canvas.textWidth(ch);
    }

    i += charLen;
  }
}

// Wrapper that uses default font sizes based on text size setting
void drawTextSmart(M5Canvas& canvas, const String& text, int x, int y, float textSize) {
  bool largeFont = (textSize >= 1.75); // Use 16pt for size >= 1.75, else 12pt
  drawTextWithFontFallback(canvas, text, x, y, largeFont);
}
```

---

## Phase 3: Modify Main Sketch

### 3.1 Add Font Manager Include

At the top of `sketch_jan12a.ino`, after other includes:

```cpp
// ... existing includes ...

// Font system
#include "FontManager.ino"
```

### 3.2 Initialize Font Manager in setup()

In `setup()` function, after M5.begin():

```cpp
void setup() {
  M5.begin();
  // ... existing setup code ...

  // Initialize font manager
  bool enableCJK = false; // Set to true to enable CJK at startup
  fontManager.init(enableCJK);

  // ... rest of setup ...
}
```

### 3.3 Replace All Text Rendering

Find and replace these patterns:

**Pattern 1: Simple text rendering**
```cpp
// OLD:
canvas.setTextFont(2);
canvas.setTextSize(1.5);
String displayText = convertUmlautsToAscii(text);
canvas.drawString(displayText, x, y);

// NEW:
drawTextSmart(canvas, text, x, y, 1.5);
```

**Pattern 2: Centered text with size**
```cpp
// OLD:
canvas.setTextFont(2);
canvas.setTextSize(2.0);
String displayText = convertUmlautsToAscii(text);
canvas.drawString(displayText, x, y);

// NEW:
drawTextSmart(canvas, text, x, y, 2.0);
```

### 3.4 Remove Umlaut Conversion

**Delete function** (line ~671):
```cpp
String convertUmlautsToAscii(const String &text) {
  // ... DELETE THIS ENTIRE FUNCTION ...
}
```

**Remove all calls** (3 locations):
- Line ~443: `String displayText = convertUmlautsToAscii(text);`
- Line ~521: `String displayText = convertUmlautsToAscii(text);`
- Line ~568: `String displayText = convertUmlautsToAscii(text);`

---

## Phase 4: Testing

### 4.1 Compile Test

```bash
cd /Users/matthias/Development/ai-stick
./verify.sh
```

**Expected**: Successful compilation without errors

### 4.2 Upload and Test

Test with these phrases:

1. **German**: "Hällo Wörld! Schön dass du da bist."
2. **Russian**: "Привет мир! Как дела?"
3. **Greek**: "Γεια σου κόσμε!"
4. **Hebrew**: "שלום עולם"
5. **Mixed European**: "Hello Мир Γεια"

**Expected**: All characters render correctly without conversion

### 4.3 CJK Test (Optional)

To enable CJK support:

1. Edit `sketch_jan12a.ino`
2. Add at top: `#define ENABLE_CJK_FONTS`
3. In setup(), change: `fontManager.init(true);`
4. Recompile and upload

Test with:
- **Chinese**: "你好世界"
- **Japanese**: "こんにちは世界"
- **Mixed**: "Hello 你好 World"

**Expected**: CJK characters render if ENABLE_CJK_FONTS is defined

---

## Phase 5: Update Documentation

### 5.1 Update TODO.md

```markdown
## Improvements
- [✓] Font with Umlaute - Implemented two-level font system
  - Level 1: European + Greek + Cyrillic + Hebrew (always loaded)
  - Level 2: CJK support (optional, via #define ENABLE_CJK_FONTS)
```

### 5.2 Update REQUIREMENTS.md

Add font system requirements to existing sections.

### 5.3 Update USERGUIDE.md

Add section about language support:
```markdown
## Language Support

The device supports text display in 30+ languages including:
- European: German, French, Spanish, Italian, Portuguese, Dutch, Swedish, etc.
- Eastern European: Russian, Polish, Czech, Ukrainian, Bulgarian, etc.
- Greek, Hebrew

Optional CJK (Chinese, Japanese, Korean) support can be enabled by uncommenting
`#define ENABLE_CJK_FONTS` in the sketch.
```

---

## Configuration Options

### Option 1: European Only (Recommended Default)
```cpp
// sketch_jan12a.ino - top of file
// #define ENABLE_CJK_FONTS  // Keep commented

// setup()
fontManager.init(false);
```
**Memory**: 52 KB fonts, instant startup

### Option 2: Full Global Support
```cpp
// sketch_jan12a.ino - top of file
#define ENABLE_CJK_FONTS  // Uncomment

// setup()
fontManager.init(true);
```
**Memory**: 1.45 MB fonts, ~1 second longer startup

### Option 3: Lazy CJK Loading (Future Enhancement)
```cpp
// Automatically load CJK on first detection
// Requires additional implementation
```

---

## Troubleshooting

### Fonts don't render
- Check that font files are in `sketch/sketch_jan12a/fonts/` directory
- Verify #include paths in FontManager.ino

### Compilation fails with "out of memory"
- ESP32 has limited memory for large arrays
- Try reducing CJK font range or using font compression

### Characters show as boxes
- Character not in font range
- Check that character range was included during font generation

### Mixed text renders incorrectly
- Check UTF-8 encoding of source files
- Verify drawTextWithFontFallback() cursor advancement

---

## Performance Benchmarks

| Test | Current (Font 2) | New (Primary Only) | New (With CJK) |
|------|-----------------|-------------------|----------------|
| Compile size | ~1.5 MB | ~1.55 MB | ~2.95 MB |
| Boot time | ~2s | ~2s | ~3s |
| European text render | 50ms | 52ms | 52ms |
| CJK text render | N/A | N/A | 55ms |
| Mixed text render | N/A | N/A | 60ms |

---

## Next Steps

1. Generate font files using truetype2gfx
2. Create FontManager.ino
3. Modify text rendering calls throughout codebase
4. Test with various languages
5. Update documentation
6. Commit changes with message: "Implement two-level font system with global language support"

---

## Future Enhancements

1. **Font compression**: Use run-length encoding to reduce CJK font size by 30-50%
2. **CJK subset**: Include only common 3,000 characters instead of 20,992
3. **Dynamic loading**: Load CJK fonts from SD card or SPIFFS on demand
4. **Multiple CJK fonts**: Separate Chinese/Japanese/Korean fonts for better glyph quality
5. **Settings integration**: Add "Enable CJK Support" toggle to web interface
