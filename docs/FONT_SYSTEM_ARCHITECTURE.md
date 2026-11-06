# Two-Level Font Lookup System Architecture

## Overview

This document describes the two-level font lookup system that provides comprehensive global language support while optimizing for performance and memory usage.

## Design Goals

1. **Fast rendering** for common European/Middle Eastern text (99% of use cases)
2. **Global coverage** with CJK support when needed
3. **Memory efficiency** through smart font loading
4. **Zero performance impact** when CJK not used

## Font Structure

### Level 1: Primary Font (Always Loaded)
- **File**: `fonts/PrimaryFont12pt.h` and `fonts/PrimaryFont16pt.h`
- **Size**: ~52 KB total (both sizes)
- **Character Ranges**:
  ```
  0x0020-0x007F  Basic Latin (96 chars)
  0x0080-0x00FF  Latin-1 Supplement (96 chars) - German, French, Spanish
  0x0100-0x017F  Latin Extended-A (128 chars) - Polish, Czech, Turkish
  0x0370-0x03FF  Greek and Coptic (117 chars)
  0x0400-0x04FF  Cyrillic (256 chars) - Russian, Ukrainian, Bulgarian
  0x0590-0x05FF  Hebrew (112 chars)
  0x2013-0x2026  General Punctuation (14 chars) - em-dash, ellipsis, etc.
  ```
- **Total**: 819 characters
- **Languages**: 30+ European and Middle Eastern languages

### Level 2: Extended Font (Lazy-Loaded)
- **File**: `fonts/ExtendedFont12pt.h` and `fonts/ExtendedFont16pt.h`
- **Size**: ~1.4 MB total (both sizes)
- **Character Ranges**:
  ```
  0x4E00-0x9FFF  CJK Unified Ideographs (20,992 chars)
  ```
- **Languages**: Chinese, Japanese, Korean
- **Loading**: On first CJK character detection or via settings flag

## Implementation Components

### 1. Font Manager (`FontManager.ino`)

```cpp
class FontManager {
private:
  const GFXfont* primaryFont12pt;
  const GFXfont* primaryFont16pt;
  const GFXfont* extendedFont12pt;  // nullptr until loaded
  const GFXfont* extendedFont16pt;  // nullptr until loaded
  bool cjkLoaded;
  bool cjkEnabled;  // From settings

public:
  void init();
  void enableCJK(bool enable);
  bool isCJKCharacter(uint16_t codepoint);
  const GFXfont* getFontForChar(uint16_t codepoint, bool large);
  void loadCJKFonts();
  void unloadCJKFonts();
};
```

### 2. Character Range Detection

```cpp
// Fast lookup table approach
bool FontManager::isCJKCharacter(uint16_t cp) {
  return (cp >= 0x4E00 && cp <= 0x9FFF);
}

bool FontManager::isPrimaryCharacter(uint16_t cp) {
  return (cp >= 0x0020 && cp <= 0x007F) ||  // Basic Latin
         (cp >= 0x0080 && cp <= 0x00FF) ||  // Latin-1
         (cp >= 0x0100 && cp <= 0x017F) ||  // Latin Ext-A
         (cp >= 0x0370 && cp <= 0x03FF) ||  // Greek
         (cp >= 0x0400 && cp <= 0x04FF) ||  // Cyrillic
         (cp >= 0x0590 && cp <= 0x05FF) ||  // Hebrew
         (cp >= 0x2013 && cp <= 0x2026);    // Punctuation
}
```

### 3. Smart Text Rendering

```cpp
void drawTextWithFontFallback(M5Canvas& canvas, const String& text, int x, int y, bool largeFont) {
  int cursorX = x;
  int cursorY = y;

  // Decode UTF-8 and render character by character
  int len = text.length();
  int i = 0;

  while (i < len) {
    uint16_t codepoint = decodeUTF8Character(text, i);

    // Select appropriate font
    const GFXfont* font = fontManager.getFontForChar(codepoint, largeFont);

    if (font) {
      canvas.setFont(font);
      // Render single character
      String ch = encodeUTF8Character(codepoint);
      canvas.drawString(ch, cursorX, cursorY);

      // Advance cursor
      cursorX += canvas.textWidth(ch);
    }

    // Skip to next UTF-8 character
    i += getUTF8CharLength(text[i]);
  }
}
```

### 4. UTF-8 Decoder

```cpp
uint16_t decodeUTF8Character(const String& text, int& index) {
  uint8_t byte1 = text[index];

  // 1-byte character (ASCII)
  if ((byte1 & 0x80) == 0) {
    return byte1;
  }

  // 2-byte character
  if ((byte1 & 0xE0) == 0xC0) {
    uint16_t cp = ((byte1 & 0x1F) << 6) | (text[index + 1] & 0x3F);
    return cp;
  }

  // 3-byte character (includes CJK)
  if ((byte1 & 0xF0) == 0xE0) {
    uint16_t cp = ((byte1 & 0x0F) << 12) |
                  ((text[index + 1] & 0x3F) << 6) |
                  (text[index + 2] & 0x3F);
    return cp;
  }

  return 0; // Invalid
}

int getUTF8CharLength(uint8_t firstByte) {
  if ((firstByte & 0x80) == 0) return 1;      // ASCII
  if ((firstByte & 0xE0) == 0xC0) return 2;   // 2-byte
  if ((firstByte & 0xF0) == 0xE0) return 3;   // 3-byte (CJK)
  if ((firstByte & 0xF8) == 0xF0) return 4;   // 4-byte
  return 1; // Fallback
}
```

## Memory Management Strategy

### Option A: Lazy Loading (Recommended)
1. Load primary fonts at startup (~52 KB)
2. Monitor text for CJK characters during rendering
3. On first CJK detection:
   - Display loading message
   - Load extended fonts from flash (~1.4 MB)
   - Cache in RAM for session
4. Keep CJK fonts loaded until deep sleep or reboot

**Pros**:
- Zero memory overhead if CJK never used
- One-time load cost amortized over session

**Cons**:
- 1-2 second delay on first CJK text
- Slight code complexity

### Option B: Settings-Based Loading
1. Add "Enable CJK Support" toggle to settings (default: OFF)
2. If enabled, load both font levels at startup
3. If disabled, only primary fonts loaded

**Pros**:
- User control over memory/feature tradeoff
- Predictable behavior

**Cons**:
- Requires user configuration
- CJK text shows as boxes if disabled

### Option C: Always Load Both (Simplest)
1. Load all fonts at startup
2. Always 1.45 MB font memory usage

**Pros**:
- Simplest implementation
- Always works

**Cons**:
- 18% flash usage even if never used

## Recommended Approach

**Use Option A (Lazy Loading)** because:
1. Most users won't encounter CJK text (German device)
2. Zero overhead for 99% of use cases
3. Automatic - no user configuration needed
4. Graceful fallback if CJK appears

## Code Locations to Modify

### Files to Create:
1. `sketch/sketch_jan12a/FontManager.ino` - Font management class
2. `sketch/sketch_jan12a/fonts/PrimaryFont12pt.h` - Primary font, 12pt
3. `sketch/sketch_jan12a/fonts/PrimaryFont16pt.h` - Primary font, 16pt
4. `sketch/sketch_jan12a/fonts/ExtendedFont12pt.h` - CJK font, 12pt
5. `sketch/sketch_jan12a/fonts/ExtendedFont16pt.h` - CJK font, 16pt

### Files to Modify:
1. `sketch/sketch_jan12a/sketch_jan12a.ino`:
   - Remove `convertUmlautsToAscii()` function (line 671)
   - Remove all calls to `convertUmlautsToAscii()` (3 locations)
   - Replace `canvas.setTextFont(2)` with font manager calls (8 locations)
   - Add `#include "FontManager.ino"` at top
   - Initialize FontManager in `setup()`

2. All text rendering functions:
   - `drawScreen()` - line 638
   - `drawScreenWithSpinner()` - line 684
   - `drawScreenCentered()` - line 556
   - Icon text rendering - lines 427-464

## Performance Impact

| Scenario | Rendering Speed | Memory Usage |
|----------|----------------|--------------|
| European text only | Same as current | +52 KB flash |
| First CJK character | 1-2s load delay | +1.45 MB flash |
| Subsequent CJK text | Same as current | +1.45 MB flash |
| Mixed text | ~5% slower* | +1.45 MB flash |

*Slower due to per-character font switching, but negligible for typical text lengths

## Testing Strategy

1. **European text**: "Hällo Wörld! Привет мир! Γεια σου κόσμε! שלום עולם"
2. **CJK text**: "你好世界"
3. **Mixed text**: "Hello 你好 World 世界"
4. **Edge cases**: Empty string, single character, very long text
5. **Memory**: Monitor heap before/after CJK load
6. **Performance**: Time rendering with/without font switching

## Migration Path

### Phase 1: Create Font Files (External)
Use M5Stack FontCreator or truetype2gfx to generate:
- Primary fonts with ranges 0x0020-0x05FF + 0x2013-0x2026
- Extended fonts with range 0x4E00-0x9FFF

### Phase 2: Implement Font Manager
Add FontManager.ino with font selection logic

### Phase 3: Implement UTF-8 Decoder
Add UTF-8 decoding and character-by-character rendering

### Phase 4: Update All Rendering Calls
Replace setTextFont(2) throughout codebase

### Phase 5: Remove Umlaut Conversion
Delete convertUmlautsToAscii() and all calls

### Phase 6: Testing
Comprehensive testing with various language combinations

## Future Enhancements

1. **Additional Scripts**: Thai, Arabic, Tamil, etc. (as needed)
2. **Font Compression**: Reduce CJK font size with run-length encoding
3. **Subset CJK**: Include only common 3,000 characters instead of 20,992
4. **Dynamic Loading**: Load CJK on per-language basis (Chinese vs Japanese vs Korean)
5. **Font Caching**: Keep recently-used glyphs in RAM for faster re-rendering

## Configuration

Add to settings system (REQUIREMENTS.md):

```cpp
### Font Settings
- **Enable CJK Support**: true/false (default: false)
  - If true, loads CJK fonts at startup
  - If false, lazy-loads on first CJK character detection
  - Configurable via web interface
```

## File Size Budget

| Component | Current | After Primary | After Full |
|-----------|---------|--------------|------------|
| Sketch .ino | ~1.5 MB | ~1.5 MB | ~1.5 MB |
| Primary fonts | 0 KB | 52 KB | 52 KB |
| Extended fonts | 0 KB | 0 KB | 1.4 MB |
| **Total** | ~1.5 MB | ~1.55 MB | ~2.95 MB |
| **Remaining** | 6.5 MB | 6.45 MB | 5.05 MB |

**Verdict**: Plenty of flash space remaining even with full CJK support.
