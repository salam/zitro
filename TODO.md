# To do

## Improvements
- [✔︎] Font with Umlaute → Two-level font system **FULLY IMPLEMENTED** ✨
  - ✅ FontManager class with UTF-8 decoder integrated
  - ✅ All text rendering updated to use custom fonts (no more ASCII conversion!)
  - ✅ `convertUmlautsToAscii()` function removed
  - ✅ Custom font files generated (PrimaryFont10pt.h 216KB, PrimaryFont14pt.h 332KB)
  - ✅ Word wrapping implemented for text that exceeds screen width
  - ✅ Code compiles successfully (1.67 MB, 49% flash)
  - ✅ Supports European languages + Greek + Cyrillic + Hebrew (character range 0x20-0x5FF)
  - 📝 Font files: sketch/sketch_jan12a/fonts/PrimaryFont10pt.h & PrimaryFont14pt.h
  - 📝 Based on Roboto Regular font with 1,504 glyphs covering 30+ languages
  - 💡 Optional Level 2 (CJK): Can be added via `#define ENABLE_CJK_FONTS` if needed
  - See: docs/FONT_SYSTEM_ARCHITECTURE.md, FONT_IMPLEMENTATION_PLAN.md, FONT_GENERATION_GUIDE.md
- Show text result already while rendering image
  - --> pulsate the image icon at the bottom right until ready

## Random
[✔︎] Jokes
[✔︎] Zufällige Idee, kreativ

## Features

- [✔︎] Reminders
  - ✅ Voice-controlled timers via GPT (set_timer, timer_minutes, timer_label)
  - ✅ Alert for tomorrow / absolute times
  - ✅ Remind me wash machine / relative times
  - ✅ Pomodoro timer (25 min)
  - ✅ List and clear timers by voice
  - ✅ Timer list view with Button B/C navigation
  - ✅ Alert sound + display even during sleep
  - ✅ Light sleep timer wakeup, deep sleep prevention
- Local note capturing
  - Notiz: abc
  - --> Sync with a todo list (iOS, Mac, Windows, or similar)
- "Local" weather
- Price look-up (food-ally)
- Profiles / Passwords
  - Ich bin Papa.
  - Ich bin ...
- Calendar
  - What's up today?
- Tenor news headlines:
  - simple, kids friendly, up-to-date, helpful, learning, local
- Throw dice
- ✅ QR-hand-off to own webpage
- Navigatable map with globe, for geographical questions 
  - --> set the pin right (based on Wikipedia?)
- Solve audio
  - make encoding and volume correct so that one can listen
  [ ] Answer with audio if there is a music question.

## Bugs:
- Resizing and correct colors
- Better and faster image generation (other model, simpler task, not only cartoon/baby pics)