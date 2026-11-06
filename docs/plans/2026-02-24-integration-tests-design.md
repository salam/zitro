# Integration Test Suite Design

**Date:** 2026-02-24
**Purpose:** Create a safety net of tests before refactoring the monolithic sketch_jan12a.ino (8,473 lines) into multiple modules with restructured logic.

## Approach

Native C++ tests compiled with g++ on macOS. Lightweight Arduino type stubs. No external dependencies.

## File Structure

```
test/
├── test_runner.cpp      # All test cases + main()
├── arduino_stubs.h      # Arduino String, Serial, Preferences, time() stubs
├── test_framework.h     # Assertion macros (TEST, ASSERT_EQ, ASSERT_TRUE, etc.)
test.sh                  # Compile with g++ and run (project root, alongside verify.sh)
```

## Arduino Stubs (arduino_stubs.h)

Reimplements just enough Arduino API to compile and test the pure logic:

- **String** — wraps std::string. Methods: charAt, substring, indexOf, replace, toInt, toUpperCase, toLowerCase, length, c_str, trim, + operator, comparisons
- **Serial** — println/printf to stdout
- **Preferences** — in-memory std::map simulating begin/end/getBool/putBool/getInt/putInt/getLong/putLong/getString/putString
- **time()/getLocalTime()** — controllable stubs with settable test time
- **PROGMEM/pgm_read_byte** — no-op macros
- Standard C lib (memcpy, strlen, snprintf) used directly

## Functions Under Test

### Tier 1: Pure Logic (String stub only)

| Function | Lines | Test Cases |
|----------|-------|------------|
| normalizeString | 2605-2619 | ASCII lowercase, uppercase→lowercase, skip punctuation/spaces, preserve extended ASCII umlauts, empty string |
| escapeJson | 7188-7196 | Backslash, quotes, newlines, tabs, \r, combined chars, empty string |
| wordWrap | 7011-7042 | Single word fits, wrap at boundary, preserve explicit \n, empty string, exact width |
| FontManager::decodeUTF8Character | 58-88 | 1-byte ASCII, 2-byte (ä=0xC3A4), 3-byte (€=0xE282AC), invalid sequence skip, end of string |
| FontManager::isSupportedCharacter | 91-93 | In range 0x20-0x5FF, below range, above range, boundaries |

### Tier 2: Global State Functions (String + controllable globals)

| Function | Lines | Test Cases |
|----------|-------|------------|
| createWavHeader | 2451-2481 | RIFF/WAVE/fmt/data markers correct, sample rate bytes, file size = dataSize+36, byte rate = sampleRate*2 |
| addIPPAttribute | 2764-2780 | Tag byte, name length big-endian, name bytes, value length big-endian, value bytes |
| addIPPAttributeInt | 2782-2799 | Tag byte, name encoding, value length=4, big-endian int32 |
| addTimer | 7514-7541 | Returns confirmation string, timer struct populated, max limit (5) returns error, formats hours+minutes |
| removeTimer | 7543-7549 | Valid removal decrements count, out-of-bounds ignored, inactive timer ignored |
| clearAllTimers | 7552-7558 | All slots inactive, count=0 |
| getNextTimerSeconds | 7562-7575 | No timers → -1, single timer, picks earliest of multiple, past timer → 0 |
| formatTimeRemaining | 7577-7595 | hours+minutes, minutes+seconds, seconds only, past time → "jetzt!" |

### Tier 3: Preferences-Dependent (String + Preferences mock + time stub)

| Function | Lines | Test Cases |
|----------|-------|------------|
| applySettingChange | 3698-3824 | Bool true/false/ein/aus/ja, int with range clamping, int with allowed values rejection, long seconds→ms, string with validation, unknown key |
| isRestrictedHours | 2622-2631 | hour=21 restricted, hour=3 restricted, hour=12 not restricted, hour=6 boundary, time not synced → false |
| getCurrentDateTimeString | 2633-2656 | Correct German day name, dd.mm.yyyy format, includes seconds, time not synced → empty |

## How Source Is Included

The test_runner.cpp #includes the actual .ino source after stubs are defined. Preprocessor guards skip hardware headers:

```cpp
// Stub out hardware includes
#define M5Unified_h
#define WiFi_h
#define WiFiClientSecure_h
#define HTTPClient_h
// ... etc

#include "arduino_stubs.h"

// Include the actual source under test
#include "../../sketch/sketch_jan12a/sketch_jan12a.ino"
```

This ensures tests run against the exact production code, not copies.

## Running

```bash
./test.sh     # Compile & run tests. Exit 0 = pass, 1 = fail
./verify.sh   # Existing Arduino compilation (unchanged)
```

## Expected Coverage

~50-60 assertions across 16 functions. These are the functions most likely to break during refactoring (string processing, data formatting, protocol encoding, timer logic, settings validation).

## What's NOT Tested

Hardware-dependent functions (WiFi, display, audio, IMU, printer communication) are covered by verify.sh compilation checks only. The refactoring should preserve their function signatures to ensure the existing verify.sh continues to pass.
