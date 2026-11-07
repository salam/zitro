# Requirements for Feature Image Downsampling 

## Hardware
- ESP32 with PSRAM (minimum 2MB)
- M5Stack or compatible device with M5GFX library

---

# Requirements for Configurable Settings System

## Settings Storage
- Uses ESP32 Preferences library with "settings" namespace
- Persistent storage across reboots and deep sleep
- Separate from WiFi credentials (stored in "wifi" namespace)

## Configurable Parameters

### API and Authentication
- **AI provider** (`provider`): `zitro`, `openai` (default), `anthropic`, `infomaniak`, `custom`
- **Keys** (Preferences, never returned to the browser; only the last 4 characters are shown):
  - `apiKey` (OpenAI), `claudeKey`, `ikKey` (Infomaniak token), `customKey`, `zitroKey`
  - Empty key fields on save keep the stored key
  - No key is compiled into the firmware
- **Provider options**: `oaModel`, `claudeModel`, `ikModel`, `ikProduct` (Infomaniak product ID), `customUrl` (must be https), `customModel`
- Device enters setup mode (AP) at boot if the selected chat provider is not configured
- Speech-to-text (`gpt-transcribe`), text-to-speech (`gpt-4o-mini-tts`) and image generation (`gpt-image-2.5-flare`, fallback `gpt-image-1.5`) use the OpenAI API, or the Zitro relay when `provider` is `zitro`
  - Non-OpenAI chat providers therefore also need `apiKey` for voice; the device shows a hint instead of recording when it's missing

### AI Provider Layer (ai_provider.h / ai_provider.ino)
- One entry point `aiChat(system, user, maxTokens, withMemory, jsonMode)` used by the main answer and the Wolfram translation
- OpenAI and Zitro: Chat Completions, `max_completion_tokens` (min 1024), `reasoning_effort: low`, JSON mode for answers
- Infomaniak: `https://api.infomaniak.com/2/ai/{product}/openai/v1/chat/completions`, `max_tokens`
- Custom: `{customUrl}/chat/completions`, `max_tokens`
- Anthropic: Messages API, `x-api-key`, `anthropic-version: 2026-10-21`, `max_tokens` >= 2048 (thinking is always on), `output_config.effort: low`, no sampling parameters; server-side refusal fallback (`fallbacks: "default"` + beta header) on Opus 5.x / Sonnet 5.5 / Fable 5.x; `refusal` stop reason shows a friendly message
- Responses are parsed with ArduinoJson (text blocks concatenated for Anthropic); the answer JSON may be wrapped in text or code fences
- Errors: 401/403 "API key rejected", 429 "Too many requests", 402 or Zitro 429 "Zitro quota reached"
- On screen, "API key rejected" adds the way to fix it: "Triple-press the side button for settings."

### Tests
- Host unit tests (no hardware): `test/host/run_tests.sh` covers request bodies per provider, response parsing (thinking blocks, refusals, HTTP errors, non-JSON bodies), answer-JSON parsing (code fences, string/float values), and the memory window (expiry, capacity, UTF-8 truncation, switches)
- Pure logic lives in `ai_payload.ino` and `conversation_memory.ino` so it compiles on the host via `test/host/arduino_shim.h`

### Conversation Memory
- Setting `memory` (bool, default on), voice- and web-configurable
- Sliding window: turns older than 10 minutes are dropped; max 5 turns; question <=160 bytes, answer <=320 bytes (UTF-8 safe truncation)
- Stored in RTC slow memory (`RTC_DATA_ATTR`): survives light and deep sleep, lost on power-off, never written to flash
- Requires NTP time; without a valid clock nothing is remembered
- Sent as alternating user/assistant messages before the new question; assistant turns replayed as `{"answer": "..."}`
- Only normal answers are recorded (not settings, timers, onboarding, Wolfram, or errors)
- Cleared when memory is switched off or the language changes

### Audio Recording
- **Recording Duration**: 1-30 seconds (default: 5 seconds)
  - Configurable via web interface
  - Affects RECORD_SECONDS and RECORD_SAMPLES globals
- **Button A during recording**: one tap restarts the recording; a second tap within 500 ms cancels it and shows the last answer (or the home screen). Further taps of the burst are swallowed.
- **Wake press**: the Button A press that wakes the device (display, light or deep sleep) never starts a recording. Pending clicks are discarded after wake, and a 2 s cooldown applies after every wake.

### Language Settings
- **Language**: DE (German) or EN (English)
  - Affects all UI strings (i18n system)
  - Appends language instruction to prompts (" Answer in English." for EN)
  - Default: DE

### Power Management Timeouts
- **Display Sleep Timeout**: 5-300 seconds (default: 30 seconds)
- **Light Sleep Timeout**: 10-600 seconds (default: 90 seconds)
- **Deep Sleep Timeout**: 30-1800 seconds (default: 180 seconds)
- All timeouts stored in milliseconds, applied at boot and immediately after a change (web or voice); enforced order display <= light <= deep
- Light sleep sets a timer wake-up at the deep sleep deadline (the inactivity check can't run while the CPU is paused), then goes straight to deep sleep without waking the screen
- Before light and deep sleep: WiFi off, display controller asleep (backlight off), IMU asleep, mic and speaker stopped
- Deep sleep latches the power-hold pin (GPIO4, the Plus2 has no PMIC) with `gpio_hold_en` so Button A can still wake the device on battery; released after boot
- Active reminder timers keep the device in light sleep with a timer wake-up instead of deep sleep
- Motion sensor while awake: accelerometer always on (shake, auto-flip, rotate-to-toggle); gyroscope (PWR_MGMT_2 standby bits) only while an image is shown, for the roll-to-zoom gesture, or a 3D model, for turning it. Roll readings are ignored for 150 ms after the gyro powers up

### Feature Toggles
- **Search Related Images**: Enable/disable Wikipedia image search (default: on)
- **Use Wolfram Alpha**: Enable/disable Wolfram for math queries (default: on)
- **Use Local Quotes**: Toggle between CSV and API for quotes (default: local CSV)
- **Parental Lock**: Enable/disable parental control (default: off)
  - Can be toggled via voice command regardless of setting

### Gesture/Interaction Toggles
- **Shake for Random Fact**: Enable/disable shake gesture for surprise facts (default: on)
- **Double-Click for Audio**: Enable/disable double-click for audio output (default: on)
  - If disabled, double-click behaves like single-click (text only)
- **Movement for Zoom**: Enable/disable roll gesture to toggle image zoom modes (default: on)
- **Movement for Toggle**: Enable/disable rotation gesture to toggle image/text view (default: on)

### Audio Settings
- **Speaker Volume**: 0-255 (default: 210)
  - Applied during speaker initialization in setup() (read from Preferences)

### Branding
- Boot splash: "zitro.spot" wordmark only, on black (240x135 PNG in `logo_data.h`), generated by `scripts/make_zitro_brand.py`; the wordmark keeps at least 24 px padding left and right and is centered vertically
- Boot jingle: two short organic pops, the second one higher (about 0.6 s, at most 1.9 s, 16-bit PCM mono 16kHz, `splash_jingle_data.h`), start with the boot splash and end before it does, so boot takes no longer. Plays on power-on and restart, not on a wake from deep sleep. Follows the Speaker Volume setting. Made with ElevenLabs by `scripts/make_splash_jingle.py` (key: `ELEVENLABS_API_KEY`, from the environment or the gitignored `.env`; only needed to make a new jingle, the stick never contacts ElevenLabs)
- Setup hotspot (open WiFi) is named "zitro"; mDNS makes the setup page reachable at `zitro.local`. The WiFi QR code (`WIFI:T:nopass;S:zitro;P:;;`, QR version 3, 93x93 px) is made by `generate_wifi_qr.py` and `convert_wifi_qr.sh`
- Setup page follows zitro.spot (webpage2/styles.css): paper palette, Sniglet headings, lemon-shaped citrus buttons. Sniglet is served from flash (`setup_fonts.h`, `/f/sniglet-400.woff2`, `/f/sniglet-800.woff2`, made by `scripts/make_setup_fonts.py`) because setup mode has no internet
- Home screen (`drawHomeScreen()`, drawn whenever `MAIN_QUESTION` is shown): "[lemon] Press [M5] button" on one line: small citrus lemon (#f2b705, with leaf), then "Press", a keycap labeled "M5" like the stick's front button (rounded dark grey face with white outline, smaller DejaVu 12 text, 2 px mid-grey key side showing bottom and right), then "button"; "to ask a question"; half-line gap; shake icon (`shakeIcon`, phone slightly tilted) + "Shake for random fact."
- Setup screen: "SETUP: Connect to:" / zitro / "and open:" / underlined "zitro.local" / "Then press BUTTON" "to retry." (retry lines only with stored networks), WiFi QR code on the right
- "Opening settings" (Button C triple-press or voice request) uses the same QR screen with "Opening settings..." / "Connect to:" as its first lines, before the hotspot starts
- Recording progress: one slightly rounded box (3 px radius) per second, filled when recorded
- Volume changes ("Lautstaerke auf N gesetzt.") show a 2x speaker icon (`volumeIcon`) left of the word-wrapped text, also when the response is redrawn later
- Mic error: 2x broken microphone icon (`micBrokenIcon`) + "Mic error"
- UI icons without a hand-drawn PNG (shake, volume, broken mic) are drawn as pixel art in `scripts/make_ui_icons.py`, then converted by `convert_icons.sh`
- Screen texts must fit 135 px height: at most 4 lines at the 12 pt font (no blank spacer lines in timer screens)
- Text screens (`drawScreen`, `drawScreenWithSpinner`): an empty line (`\n\n`) takes half a line height. If a text (e.g. a long fact) is taller than the screen, the line height shrinks step by step from font height + 4 px (27 px) down to font height - 3 px (20 px) until it fits

### Print Screens
- Print flow screens are icon-based, no instruction text: printer icon (`print_icon_20.png` → `printIcon`) plus one or two words ("Searching...", "Printing...", "Sent!", "Print failed", "No printer found"); result screens add a check or cross badge
- Printer list: one row per printer with the printer icon in front of the name. Name: Bonjour service instance name (e.g. "Canon TS5100 series"), else TXT `ty` (make and model), else the hostname without ".local"; "_"/"-" shown as spaces, cut to fit; selected row is a white rounded bar; 3 rows visible, scrollbar when more
- Print screens follow how the stick is held: landscape (rotation 1/3, from accel x) or portrait (rotation 0/2, from accel y, `getPortraitRotation()`); the timer screens (set, alert, list) do the same; all other screens stay landscape
- Portrait printer list: one printer at a time as a white card (2x printer icon, name word-wrapped to up to 3 lines), position dots on the side away from Button A; status screens put the icon above the text
- Button hints are tabs at the screen edge next to the physical button:
  - Rotation 3 (landscape, buttons left): A ✓ left-middle, C ▼ top-left, B ✕ bottom-center
  - Rotation 1 (landscape, buttons right): A ✓ right-middle, C ▼ bottom-right, B ✕ top-center
  - Rotation 0 (portrait, A at the bottom): A ✓ bottom-center, C ▶ left edge low, B ✕ right-middle
  - Rotation 2 (portrait, A on top): A ✓ top-center, C ▶ right edge high, B ✕ left-middle
  - The C (next) tab is hidden with only one printer
- Button C triple-press (open settings) only counts where Button C has no other job (`buttonCCountsForSettings()` in `button_rules.h`): not in the printer list, the timer list or a timer alert (C is "next"), and not while the answer has a picture or 3D model (C scrolls or switches views). There the settings open by voice
- While the printer list is open, shake, rotate-to-toggle and roll are ignored; turning the stick (every 200 ms check) redraws the list in the new orientation, and does not count toward the text/image toggle

### Print Pages
- After the printer list, Button A opens a size list; Button A there prints (Zoom opens a percentage picker first). Button B goes one step back, Button C steps through the choices. Same tabs, rotations and gesture guards as the printer list; the last size and zoom are remembered (Preferences namespace `print`)
- Sizes: Fit to page (contain), Fill page (cover, cropped), Half + text, Half page, Zoom 10/25/33/50/66/75/100/125/150/200% of the Fit size (centered, cropped evenly above 100%). Each row has a page pictogram showing the picture's position
- Half + text / Half page: picture contained in the top half of the sheet. Half + text puts the answer in small text (Roboto 14 pt glyphs x2, about 2.7 mm lines) starting just below the middle
- Every page ends with a grey footnote (Roboto 10 pt glyphs x2) in the device language: image credit, "Printed with" / "Gedruckt mit", the home screen lemon in colour, underlined https://zitro.spot. Credit: "Wolfram|Alpha", "AI-generated" / "KI-generiert", or Wikipedia/Wikimedia Commons plus the link to the file's description page, derived from `currentImageUrl`
- Pages are rendered on the device as one A4 page, 8-bit PWG raster (`pwg_raster.h`), streamed line by line: nearest-neighbour scaling only, so a page stays small (about 0.1-0.6 MB). Margins 10 mm top and sides, 14 mm bottom; text keeps its size on paper at any resolution
- Before printing, the stick reads `pwg-raster-document-resolution-supported` and `pwg-raster-document-type-supported` and plans up to 4 attempts: 600 dpi first, then lower resolutions, then higher (up to 1200); sRGB before grey. Printers that report nothing get 600, 300 in colour, then in grey. Non-square resolutions are skipped
- The next attempt only follows a rejection, which leaves no job behind: HTTP 4xx, IPP client error or operation-not-supported, the printer hanging up mid-upload, or running out of memory. No retry when unreachable, busy, not authorized or without an answer, since it may be printing: those show "Print failed"
- Sent with Content-Length (counted in a first pass), never chunked: the Canon TS5100 stops on chunked uploads. Replies may be chunked. Success means HTTP 200 and an IPP successful-* status
- Printers without `image/pwg-raster` or a usable type, all attempts rejected, or no memory for the picture: the printer gets the image file as before, no layout, no footnote
- Tests without paper: `tests/pwg/run.sh` checks the encoder and capability parsing against libcups; `tests/pwg/fake_printer.py` poses as a printer (options act out other printers) for the serial commands `printtext`, `printimg`, `printtest`, and `printtest caps` reads a real printer without printing

### Animated GIFs
- `gif_anim.ino` plays GIFs on the image view, `gif_decode.h` does the decoding and compositing: offset partial frames, transparency, disposal methods, local palettes, interlacing, frame delays, loop counts (no loop block: plays once and the last picture stays)
- Delays of 0 or 1 hundredths of a second count as 100 ms, like browsers. A frame slower than its delay slows the playback down
- Still first frame instead: one frame, cut off by the 300 KB download limit (no trailer), over 120000 pixels (`GIF_ANIM_MAX_PIXELS`, about 350 x 350) or no memory. Larger frames take 180 ms or more to decode, and buttons are only read between frames
- Playback starts in `displayImage`, ticks at the top of `loop()`, and stops and frees its memory on any other full-screen draw (hook in `captureCanvas()`) or when the picture is replaced. Redraws of the same GIF (scroll, zoom, rotation, wake) keep the current frame. It pauses during display sleep and never counts as activity, so the sleep timeouts stay as they are
- Transparent GIF pixels keep what is underneath: black on the screen, white on printed pages
- Tests: `tests/gif/run.py` compares every frame with Pillow on the host; `tests/gif/device_check.py` compares what a connected stick shows

### Screenshots
- Full-screen draws pass their canvas to `captureCanvas()`; the serial commands `tour` and `shot` stream run-length encoded RGB565 frames, decoded by `scripts/capture_screens.py` into `screenshots/states/`
- App icon source: `designs/zitro-app-icon.png`

## i18n (Internationalization) System

### Supported Strings
- Main question prompt
- Recording status
- Transcribing status
- Thinking status
- Calculating status (Wolfram)
- Creating image status
- Finding image status
- No camera message

### Language-Specific Prompt Handling
- German (DE): No prompt modification (system default)
- English (EN): Appends " Answer in English." to user queries
- The device introduces itself as "Zitro" in the system prompt

## Voice-Controlled Settings
- All settings (except API key) are configurable through voice commands
- Uses GPT to detect setting change intent from natural language
- Dynamic settings registry (`settingsRegistry[]`) defines all voice-configurable settings
  - Adding a new entry to the registry automatically makes it voice-configurable
  - Each entry defines: key, type, display name (DE/EN), validation constraints
- GPT response includes `is_setting`, `setting_key`, `setting_value` fields
- `buildSettingsPrompt()` dynamically generates prompt with current values
- `applySettingChange()` validates and persists changes, hot-applies where possible
- Hot-applied settings: volume, sample rate, recording duration, parental lock
- Settings read from Preferences on use: sleep timeouts, feature toggles, language
- Parental lock during restricted hours still allows settings changes (to disable lock)

## Web Interface Requirements

- Settings form with all configurable parameters
- Real-time loading of current settings via `/settings` endpoint
- Settings save via `/savesettings` endpoint
- Settings are read from Preferences on use; only adding a WiFi network restarts the device
- Validates input ranges against `settingsRegistry` (clamped to min/max, enum values checked)
- Tabs: WiFi, AI (provider + keys + models), Settings; zitro.spot visual style (paper #f4efe6, ink #2b2721, citrus #f2b705), offline-safe fonts
- `settingsRegistry` entries carry a default (`defVal`) used by the web UI, the GPT settings prompt and `/settings`

## Libraries
- **M5GFX**: JPEG decoding, sprite buffer management
  - Must support `setPsram(true)` for PSRAM allocation
- **JPEGENC**: JPEG encoding (quality 80)
  - Enable with `#define ENABLE_JPEG_CONVERSION`

## Memory Constraints
- Regular heap: ~120KB free (insufficient for 1024x1024 JPEG at ~250KB)
- PSRAM: 2MB total, ~1.8MB free (sufficient for temporary buffers)
- Max input JPEG: 350KB
- Downsample threshold: 200KB

## Processing Pipeline
1. **Download**: Base64 decode 1024x1024 JPEG into PSRAM (up to 350KB)
2. **Decode**: M5Canvas sprite in PSRAM (2MB RGB565 buffer)
3. **Downsample**: Factor-4 sampling to 256x256 (128KB in PSRAM)
4. **Free**: Delete canvas sprite immediately (reclaim 2MB)
5. **Re-encode**: RGB565→RGB888→JPEG at Q80 (~15-30KB in regular heap)
6. **Fallback**: On error, retry with `gpt-image-1.5` (dall-e-2 was removed from the OpenAI API in May 2026)

## Memory Allocation Strategy
- Input JPEG >200KB: Allocate in PSRAM using `ps_malloc()`
- Canvas sprite: Use `setPsram(true)` before `createSprite()`
- Avoid duplicate buffers: Use canvas buffer directly, don't copy
- Sequential allocation: Free 2MB canvas before allocating other buffers

## Critical Findings
- Cannot allocate 4MB simultaneously (2MB canvas + 2MB copy)
- PSRAM fragmentation can cause allocation failures
- M5GFX `setPsram(true)` is required for large sprites
- RGB565→RGB888 conversion needed for JPEGENC input

---

# Requirements for Power Management & Sleep Modes

## Sleep States
The device implements three-tier power management to maximize battery life:

1. **Display Sleep** (30 seconds of inactivity)
   - Display turns off
   - Sensor detection paused (orientation, shake, roll gestures)
   - WiFi reconnection checks paused
   - CPU remains active but idle

2. **Light Sleep** (90 seconds of inactivity)
   - WiFi powered off
   - CPU paused
   - RAM retained
   - Instant wake-up on button press

3. **Deep Sleep** (180 seconds / 3 minutes of inactivity)
   - Maximum power savings
   - Device reboots on wake
   - Previous answer and image URL saved to Preferences
   - Image re-downloaded after wake
   - AI-drawn pictures have no URL, so they are saved to SPIFFS (`/lastimage`, up to 512 KB) just before deep sleep and read back on wake; the file is removed once read. The partition is formatted on first use. If saving or reading fails, the answer comes back without a picture, never with "No image buffer". Wolfram pictures aren't saved, because its terms forbid storing results

## Wake-Up Behavior

**Critical Requirement**: When waking the device from any sleep state by pressing Button A, the button press must ONLY wake the device and restore the display. It must NOT trigger a new recording/transcription.

### Implementation Requirements:
- Wake-up button press must be consumed and not propagated to the main loop
- After waking from light sleep, call `M5.update()` to clear button state
- Button handlers must check `wakeFromLightSleep` flag and return early
- First button press after wake: displays previous result only
- Subsequent button presses: work normally (record, transcribe, query)

### Wake-Up Flow:
```
1. User presses Button A during sleep
2. Device wakes from sleep state
3. Display turns on immediately (light sleep) or boots up (deep sleep)
4. Button state cleared via M5.update()
5. Previous screen content restored
6. wakeFromLightSleep flag prevents recording
7. Next button press works normally
```

## Button Handler Guards
All button click handlers must include these guards:
```cpp
if (displaySleeping || currentPowerState != POWER_ACTIVE || wakeFromLightSleep) {
  // Wake-up logic: restore display, return early
  return; // Don't trigger recording on wake-up click
}
```

## Power State Transitions
- Active → Display Sleep: Automatic after 30s inactivity
- Display Sleep → Light Sleep: Automatic after 90s total inactivity
- Light Sleep → Deep Sleep: Automatic after 180s total inactivity
- Any Sleep State → Active: Button A press (wake-up only, no recording)

## Sensor Pausing During Sleep
When `displaySleeping == true` OR `currentPowerState != POWER_ACTIVE`:
- `detectShakeGesture()` must not be called
- `detectOrientation()` must not be called
- `detectRollGesture()` must not be called
- `checkWiFiConnection()` must not be called

This prevents unnecessary CPU usage and IMU sensor reads during sleep states.

---

# Requirements for Website Cache Busting

## JavaScript Version Management

When modifying JavaScript files, browsers may serve cached versions to users, preventing them from seeing updates. To solve this, use version parameters in script tags.

### Version Parameter Format
- All JavaScript file references must include a version query parameter
- Format: `<script src="filename.js?v=X.X.X"></script>`
- Version follows semantic versioning: MAJOR.MINOR.PATCH

### Affected Files
1. **webpage/index.html**: `<script src="script.js?v=X.X.X"></script>`
2. **webpage/shop.html**: `<script src="shop.js?v=X.X.X"></script>`

### Version Increment Rules
- **Patch (X.X.1)**: Bug fixes, minor code changes, small improvements
- **Minor (X.1.0)**: New features, significant functionality additions
- **Major (1.0.0)**: Breaking changes, major rewrites, API changes

### Implementation Requirements
1. When ANY modification is made to `script.js` or `shop.js`, increment the version number
2. Update the version parameter in the corresponding HTML file(s)
3. Document version changes in RELEASE_NOTES.md
4. Both HTML files must be updated if the change affects shared functionality

### Current Versions
- script.js: v1.0.0
- shop.js: v1.0.0

### Example Update Flow
```
1. Modify script.js to add new feature
2. Increment version: 1.0.0 → 1.1.0
3. Update index.html: <script src="script.js?v=1.1.0"></script>
4. Document in RELEASE_NOTES.md
5. Deploy all changed files together
```

### Why This Matters
- Users with cached JavaScript won't see bug fixes or new features
- Forms may submit to wrong endpoints if backend changes but JS is cached
- Critical security updates may not apply if old JS is served
- Cache busting ensures all users get the latest version immediately

---

# Requirements for the 3D View

Design: `docs/superpowers/specs/2026-11-05-3d-view-design.md`. Molecules are described here; every other kind of result is a plug-in with its requirements in its own file:

- **Music on an endless band**: [docs/features/music.md](docs/features/music.md)
- **Function graphs**: [docs/features/graph.md](docs/features/graph.md)
- **Recipes step by step**: [docs/features/recipe.md](docs/features/recipe.md)
- **Globe with a pin**: [docs/features/globe.md](docs/features/globe.md)
- **Proteins, DNA and crystals**: [docs/features/protein.md](docs/features/protein.md)
- **Night sky**: [docs/features/sky.md](docs/features/sky.md)
- **Physics scenes and geometric solids**: [docs/features/physics.md](docs/features/physics.md)
- **Timelines with pictures**: [docs/features/timeline.md](docs/features/timeline.md)
- **Organs, skeleton and cells**: [docs/features/anatomy.md](docs/features/anatomy.md)
- **Wolfram results**: [docs/features/wolfram.md](docs/features/wolfram.md)

## Plug-ins (`edu_feature.h`)
- Each kind fills in one `EduFeature` in `feature_<kind>.ino` with pure logic in `<kind>_core.h`, tested by `tests/<kind>/run.sh`; all are registered in `feature_registry.ino`. The AI prompt collects each feature's `promptRule`
- `model_view.ino` owns the sprite, the gyro, the sleep rules and the bottom line; `fetchModel()` in `model_source.ino` dispatches by kind
- Prepared data lives in `webpage2/data/<kind>/` and is read from `https://zitro.spot/data/<kind>/`. It's static hosting, uploaded separately from the firmware. Without it those kinds fall back to the picture
- `gyroToScreen()` maps the sensor's axes to the screen's: in landscape the sensor's x runs along the short side

## Honest content
- The AI only names what to look up, in the answer field `model` (`"molecule:<English name>"`, empty for none). It never supplies coordinates, IDs or numbers
- The on-screen label is the source record's title (PubChem Title), so a wrong lookup is visible
- The written answer stays AI-written
- `model` only for questions about a substance's structure or for explicit 3D requests. Photos stay the default. `create_image` and calculations (`is_wolfram`) win over `model`
- Not available: the normal Wikipedia picture (`image_query` is always filled). A found model replaces the picture, so such an answer can't be printed yet

## Lookup (`model_source.ino`)
- PubChem PUG REST, `/compound/name/<name>/SDF?record_type=3d` and `/property/Title/JSON`, generic User-Agent, no keys
- 10 s for the whole lookup, body limit 64 KB (PSRAM), V2000 SDF only, must end in `M  END`
- Up to 200 atoms and 250 bonds, otherwise not available. Atoms, bonds and hydrogens are never dropped

## View (`model_view.ino`, pure logic in `model_core.h`)
- `DisplayMode` `SHOW_MODEL`. An answer with a model opens straight in the view. Button C goes to the text and back, rotate-to-toggle enters from the text, Button B click zooms (3 steps), Button B hold does nothing yet, 3 s still resets WiFi; three quick Button C presses do not open settings here
- Shake, roll, rotate-to-toggle and landscape flip are off; rotation locks on entry
- One non-blocking frame per `loop()`, at most 25 per second. Measured on the Plus2 at full speed: 50 atoms 58-62 fps, 100 atoms 51-55 fps, 200 atoms 42-46 fps
- Gyro integrated, 2x gain. It turns by itself at 0.35 rad/s only once the hand has stayed under 3 deg/s for 3 s (`modelIdleTurnAllowed`, `button_rules.h`), so what the hand turned to stays in view. The anatomy cutaway's rocking waits the same way (`modelIdleTurning`). Opening the view counts as long ago: lying still, it turns at once. Requested by the user on 2026-11-07
- Button tabs in every result view, like the print menu's: for 3 s when the view opens (also after Button C back from the text and after a wake) and for 3 s after each Button B or C press, then gone so the content has the whole screen. Button A: microphone (new question), filled. B and C: the feature's `hints()`; without one, a magnifier for zoom (no `buttonB`) and text lines for the written answer (no `buttonC`). Paging views (recipe, timeline, Wolfram) show ▲ back, none on the first page, and ▼ next, or text lines on the last page. Anatomy shows a half-filled circle while Button C leads to the other view. Physics scenes show ▼ for Button B when the scene has more than one setting. Music: pause or play, and the volume speaker. Tabs cover the content for those 3 s, the bottom line included. Hold and double-click actions have no tab. A view that keeps its picture (Wolfram) redraws once when the tabs go (`modelViewRedraw`). Requested by the user on 2026-11-07
- The sprite (internal RAM, PSRAM as fallback) lives only while the view is on screen: freed for questions, timer alerts and setup. No memory: a short note, then the text
- Atoms in PubChem colours (`atom_table.h`, made by `scripts/make_atom_table.py`), dark rim for depth, no highlight on small atoms because it reads as a + sign. Footer: label, source, element letters in colour; a smaller ASCII font unless the label needs the full font

## Sleep
- Motion from 15 deg/s counts as activity, for up to 5 minutes after the last button press, entry or wake. Measured gyro noise on a desk: peak 0.62 deg/s. Motion never wakes a sleeping display
- Deep sleep saves the reference (`modelRef`) and fetches the model again on wake; on failure, text

## Tests
- `tests/model/run.sh`: reference parsing, SDF parsing with real PubChem files and broken input, rotation, depth order, colours
- Serial commands: `model <kind:query>`, `ask <question>` (typed question instead of the mic), `forget` (clear conversation memory), `pressb`, `pressbb` (double click), `pressc`, `holdc` (the view's button actions), `speaker` (volume and whether a tone is sounding), `modelbench <atoms>`, `modelgyro`, `modelhost <host>`, `modelnomem 1`

---

# Requirements for Timer/Reminder System

## Overview

Voice-controlled timer and reminder system. Users set timers via natural language, list them, and get alerted with sound and display when they fire.

## Timer Storage

- Maximum 5 concurrent timers in RAM (not persisted across deep sleep)
- Each timer stores: active flag, fire time (epoch seconds), label text
- Timer count tracked globally for quick checks

## Voice Commands (via GPT)

- **Set timer**: "erinnere mich in X Minuten an Y", "Timer auf X Minuten", "set pomodoro timer", "alert for tomorrow 5:30am"
  - GPT returns `set_timer: true`, `timer_minutes` (minutes from now), `timer_label` (description)
  - Pomodoro = 25 minutes
  - GPT computes minutes from current time (provided in prompt context)
- **List timers**: "liste Timer", "list pending timers", "welche Timer laufen"
  - GPT returns `list_timers: true`
- **Clear timers**: "lösche Timer", "cancel all timers"
  - GPT returns `clear_timer: true`

## Timer Set Confirmation

- After a timer is added: white rounded card with stopwatch icon (2x) plus check badge, label (10 pt, up to 2 lines) and "in <duration>" (14 pt, no trailing dot)
- The spoken/stored response text stays "Timer: <label> in <duration>"; `drawScreen()` draws the card whenever that exact response is redrawn (wake, rotation)
- Portrait (stick upright): stopwatch on top, label (10 pt, up to 3 lines) and time (14 pt, 10 pt if too wide) centered below; the card follows the stick (checked every 200 ms)
- Errors (timer limit, no free slot) stay plain text

## Timer Alert Behavior

- When a timer fires: wake display, show bell icon (3x `bellIcon`) + label (14 pt, up to 2 lines; longer labels 10 pt, up to 3 lines), play alert sound
- The alert screen blinks: full white screen with black content and full black screen with white content swap every 500 ms until dismissed; blinking stops while the display sleeps
- The bell swings with the blink: turned -10° (white frame) and +10° (black frame) around its hanger (`drawIconRotated()`)
- Portrait: bell above the label (14 pt, up to 3 lines; longer labels 10 pt, up to 4 lines), clear of the A tab
- ✓ tab next to Button A (same edge tabs as the print screens); no instruction text
- Alert sound: embedded WAV (16-bit PCM mono 16kHz, ~0.93 seconds)
- Alert stays on screen until Button A is pressed to dismiss
- During alert, all other button actions are blocked

## Timer List View

- One timer per screen as a white rounded card: stopwatch icon (2x `timerIcon`), label (10 pt), remaining time (14 pt), dots for index/total when more than one timer
- Landscape: dots centered horizontally in the card. Portrait: stopwatch above label (up to 2 lines) and time, dots above or below the card on the side away from Button A (like the printer list)
- Remaining time redraws every second (also follows the stick: landscape flip, portrait)
- Edge tabs next to the buttons, same layout as the printer list: ✓ Button A (back), ▼ Button C (next, hidden with one timer), trash can Button B (delete)
- Button C: navigate to next timer
- Button B: delete the currently displayed timer
- Button A: exit list view, return to previous screen

## Sleep Mode Interaction

- Active timers prevent deep sleep (device stays at light sleep maximum)
- Light sleep uses `esp_sleep_enable_timer_wakeup()` to wake for next timer
- Timer fires trigger display wake and alert even from sleep states
- NTP time required: timers won't fire if time is not synced (epoch < 2024)
