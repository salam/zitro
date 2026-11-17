# Requirements for Supported Boards

- **M5StickC Plus2**: ESP32, 8 MB flash, USB through a WCH USB serial chip (USB vendor ID `0x1A86`). The installer puts Zitro on it
- **M5Stack StickS3**: ESP32-S3 on its own USB (Espressif, vendor ID `0x303A`). Zitro runs on it from the same sketch; the installer can't install on it yet, a separate StickS3 installer is in the works. zitro.spot already offers both sticks (see "Requirements for Get Zitro and the Shop")
  - Build: `board.h` picks the board (`ZITRO_STICKS3` when `CONFIG_IDF_TARGET_ESP32S3`). ESP32 core 3.3.6 has no StickS3 board entry, so `verify.sh` builds `esp32:esp32:esp32s3` with 8 MB flash, OPI PSRAM, USB CDC on boot and the Plus2's partition table (`default_8MB`), next to the Plus2 build
  - Libraries: M5Unified 0.2.12 breaks the StickS3: its microphone is never switched on (fixed in 0.2.13), ESP32-S3 audio samples come out of order (0.2.20), its clock chip is driven as the wrong part (0.2.27). The StickS3 build takes M5Unified 0.2.29 and M5GFX 0.2.36, pinned by commit (`scripts/sticks3_libraries.sh`, used by `verify.sh`), and refuses to compile with an older M5Unified. The Plus2 keeps the installed versions until it has been tested with the new ones
  - Buttons: A and B only, because the power button belongs to the power chip (M5PM1) and the firmware can't read it. Button B does Button C's job too: one click is B, a double click is C, three quick clicks open the setup page (where three presses of C would), holding B is holding C where holding B has no job of its own (music: back to the text). A single click on B counts once the double-click window has passed. Every B and C read goes through `buttonBClicked()`, `buttonCClicked()`, `buttonCHeld()`, `buttonBDoubleClicked()` and `setupPressesDone()`. The Plus2's double click on B in a view (coloured note heads in music) isn't there on the StickS3
  - Button tabs (`drawButtonHints`): B's tab sits next to the StickS3's B, which is low on its side, not in the middle: measured 1 to 9 mm above the bottom edge of the 25 mm display, centre 192 px from the top with the stick upright (`BUTTON_B_FROM_TOP` in `board.h`). Rotation 3: bottom edge, 48 px from the left; rotation 1: top edge, 192; rotation 0: right edge, 192; rotation 2: left edge, 48. The tab is as long as the key and 6 px more, 82 px (`BUTTON_B_LENGTH`, 151 to 233 px from the top when upright; rotation 3: x 7 to 89); the other tabs stay 34 px (user at the stick, 2026-11-16: "so breit wie die physische Taste", then "make it 2px wider, same x-position", then "still 4px too narrow. make it 4px wider"). C's tab is never drawn, because no readable button sits there: C's job is a double click on B, and double clicks have no tab. Content moves out of the way where B's tab now sits: the tour's page dots go to the top left in rotation 1, its moon credit to the bottom right in rotation 3, the shop's row of stars 11 px up in rotation 3, and the low-battery boxes in rotation 1 from the top right corner to just left of the tab (x 128). Requested by the user on 2026-11-16, until the button module (`docs/superpowers/specs/2026-11-14-button-ui-module-design.md`) replaces it
  - Speaker volume capped at 75 % (191 of 255), because the StickS3 can restart on battery above that (M5Stack's documentation)
  - Microphone and speaker share the StickS3's codec (ES8311) and its I2S pins: `micStart()` stops the speaker while recording. A microphone that doesn't start ends the recording with an error
  - 5V output for Grove and Hat off (`cfg.output_power = false` before `M5.begin()`; M5Unified switches it on by default): the power chip's boost converter for it (M5PM1 register 0x06 bit 3) hums audibly the whole time it runs, and Zitro uses neither port. Accessories on Grove or Hat get no power from the stick. The serial command `speaker` prints `5V output 0`
  - The speaker runs only around a sound: a running speaker keeps its amplifier (M5PM1 GPIO3) on and its I2S clocks going, which hums quietly even in silence, once the 5V converter is off (user's hearing test, 2026-11-15). `speakerOffWhenQuiet()`, called at the top of `loop()`, stops it once nothing has sounded for 3 s (`SPEAKER_IDLE_OFF_MS`), long enough to keep it on between a song's notes. Each sound starts it again (the sound sites call `M5.Speaker.begin()`, and M5Unified 0.2.29 also starts it on `playRaw()` and `tone()`); `micStop()` leaves it off. The serial command `speaker` prints `running 0` at rest. The Plus2's buzzer is silent at rest and keeps running
  - No GPIO4 power latch (on the StickS3 it is the Hat2-Bus boot pin) and no MPU6886 gyro standby register (its BMI270 keeps the gyro on; `M5.Imu.sleep()` still switches it off for sleep)
  - Development build with `-DZITRO_MIC_DIAG` (`mic_diag.ino`): serial commands `hwinfo`, `micdiag <seconds> <r|l|s>` (raw samples as hex) and `asktest` (a recorded question through Whisper), and no sleep. Never in a release

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
  - `apiKey` (OpenAI), `claudeKey`, `ikKey` (Infomaniak token), `customKey`, `zitroKey`, `azKey` (Azure Speech)
  - Empty key fields on save keep the stored key
  - No key is compiled into the firmware. Open item: the Wolfram AppID from the untracked `local_secrets.h` still is (awaiting a decision)
- **Provider options**: `oaModel`, `claudeModel`, `ikModel`, `ikProduct` (Infomaniak product ID), `customUrl` (must be https), `customModel`, `ikSpeech` (bool, default off: speech-to-text at Infomaniak too), `ttsProv` (`openai` default, or `azure`), `azRegion` (default `switzerlandnorth`), `azVoice` (default `de-DE-SeraphinaMultilingualNeural`)
- Device enters setup mode (AP) at boot if the selected chat provider is not configured
- Speech-to-text (`gpt-transcribe`), text-to-speech (`gpt-4o-mini-tts`, unless Azure reads aloud, see below) and image generation (`gpt-image-2.5-flare`, fallback `gpt-image-2.6-flare`) use the OpenAI API, or the Zitro relay when `provider` is `zitro`
  - Non-OpenAI chat providers therefore also need `apiKey` for voice; the device shows a hint instead of recording when it's missing
  - Exception: with provider `infomaniak` and `ikSpeech` on (setup page, AI tab: "Transcribe my voice at Infomaniak too (Switzerland)"), speech-to-text goes to Infomaniak, so the voice stays in Switzerland like the question. Voice input then needs no OpenAI key; read-aloud and pictures still do, and the setup page says so
    - Upload: `POST https://api.infomaniak.com/1/ai/{ikProduct}/openai/audio/transcriptions`, multipart like OpenAI's, model `whisper`, `response_format=json`, `prompt` when given. No `language` field: Infomaniak's reference says a language translates the transcript (a German word in an English vocabulary round would come back in English)
    - The reply is a `batch_id`; the stick asks `GET /1/ai/{ikProduct}/results/{batch_id}` every 0.4 s, at most 30 s, over one TLS connection. `pending`/`processing` waits, `success` carries the transcript in `data` (Whisper JSON as a string, or plain text), anything else is "No transcription"
    - Product IDs must be digits (they go into the request line). Parsing lives in `stt_core.h`, host tests in `tests/stt/run.sh`
    - Not yet tested against a live Infomaniak account
  - Read-aloud can go to Microsoft Azure Speech instead (setup page, AI tab, "Read-aloud": `ttsProv` = `azure`), whatever answers the questions. OpenAI retires every model `/v1/audio/speech` can use on 2027-01-06, and its successor only works over WebRTC/WebSocket
    - `POST https://{azRegion}.tts.speech.microsoft.com/cognitiveservices/v1` with `Ocp-Apim-Subscription-Key`, `User-Agent`, SSML, `X-Microsoft-OutputFormat: raw-24khz-16bit-mono-pcm`: the same raw PCM as OpenAI's, played the same way. Azure processes the speech in the resource's region (`switzerlandnorth` is Zurich)
    - Answers: the family's `azVoice`; the default multilingual voice detects the language of the answer. `de-CH-LeniNeural`/`de-CH-JanNeural` have a Swiss accent but read only German
    - English practice (vocabulary trainer, shop game): a multilingual English voice per shop person (no "Turbo" voices, Switzerland North has none) with `<lang xml:lang='en-GB'>` for a British accent; German vocabulary words get `<lang xml:lang='de-DE'>` on a multilingual family voice ("Hand" alone could pass for English). `<lang>` holds `<prosody>`, as Azure's SSML reference allows. Pace by `prosody rate`: answers -5 %, vocabulary words -10 %, slow shop hints -20 % (small, the stick buffers about 4 s of audio)
    - When Azure refuses a voice (HTTP 400), one more try with a plain voice every region has: `de-CH-LeniNeural` for answers, `en-GB-SoniaNeural` for English
    - No quiet switch to OpenAI when the Azure key is missing: Zitro answers in writing, and the setup page says read-aloud needs the Azure key
    - `azRegion` must be lowercase letters and digits (spaces are removed, letters lowered on save), `azVoice` letters, digits, `-` and `:`; both are checked again before use. Logic in `tts_core.h`, host tests in `tests/tts/run.sh`
    - The response is read chunked, with a Content-Length, or until the server closes; header names in any case
    - Not yet tested against a live Azure Speech resource
- Wikipedia and Wikimedia requests (search, page summary, picture download) send `User-Agent: Zitro.spot/1.0 (https://zitro.spot; hello@zitro.spot)` (`ZITRO_USER_AGENT`): Wikimedia's API policy asks for a client name and a way to reach its operator. No personal address goes out

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
- **Button B during recording** (both recorders, `recordAudio()` and `recordSpeech()`): cancels at once, like A's double tap. `releaseButtonBAfterCancel()` waits for B's release and forgets its clicks, so it doesn't open the home menu afterwards, and a long hold doesn't reset WiFi. Requested by the user on 2026-11-16
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
- Motion sensor zero: M5Unified's `Imu.begin()` (0.2.12 and 0.2.29) takes one single reading as the zero offset when none is saved in NVS and subtracts it from then on. `imuForgetStartZero()` clears it after `M5.begin()`, after the `Imu.begin()` in setup and after the one on every light-sleep wake, and logs what it would have subtracted (`IMU zero from <when> cleared: gyro x,y,z dps, accel x,y,z g`). The sensor's own zero is used instead: 0.1-0.4 deg/s measured on a Plus2, 0.25 deg/s on a StickS3, both well under the 3 deg/s still limit. Found 2026-11-08: a Plus2 lying still read 76 deg/s and 1.19 g, so the star map and the globes spun by themselves and couldn't be steered; after a restart it read 0.26 deg/s and 1.00 g. Serial `modelgyro` prints the per-axis mean and the library's offset, which must be 0

### Feature Toggles
- **Search Related Images**: Enable/disable Wikipedia image search (default: on)
- **Use Wolfram Alpha**: Enable/disable Wolfram for math queries (default: on)
- **Use Local Quotes**: Toggle between CSV and API for quotes (default: local CSV)
- **Parental Lock**: Enable/disable parental control (default: off)
  - Can be toggled via voice command regardless of setting

### Gesture/Interaction Toggles
- **Shake for a Surprise** (`shakeRandom`): Enable/disable the shake gesture (default: on). Read when a shake happens; off means shakes are ignored. A shake shows a built-in fact or, half the time, a built-in result from `SHAKE_MEDIA` in `shake_core.h` (physics and astronomy scenes except the two that make sound, solids, star maps, the world clock for one of the 80 biggest cities). These need no network and open at once; if one can't open, the fact comes. Each entry is checked against its feature's closed list by `tests/shake/run.sh`. Changed from facts only on the user's request (2026-11-08)
- **Double-Click for Audio**: Enable/disable double-click for audio output (default: on)
  - If disabled, double-click behaves like single-click (text only)
- **Movement for Zoom**: Enable/disable roll gesture to toggle image zoom modes (default: on)
- **Movement for Toggle**: Enable/disable rotation gesture to toggle image/text view (default: on)

### Home City and Time Zone
- **Home city** (`homeCity`, default Zurich): a city from the world clock's table or, since 2026-11-15 on the user's request, any place `zitro.spot/api/place.php` knows. A place is kept with its name, region, IANA zone, POSIX rule and position (`homePlace`, `ClockHomeRecord` in `clock_place.h`), so it works offline at boot. Sets the device time zone with its real daylight saving rule (`configTzTime`, at boot, NTP start and on change), is "here" for `clock:here`, the house on the globe views, the place for the seasons and the Sun's path, and picks the helpline (below). Web page field with suggestions from `/cities`: a name not in the table waits in `homePending` until the stick is online (setup mode has no internet), then is looked up once in the background; until then home stays as it was, and a name zitro.spot doesn't know is reported on the page (`homeMiss`). Voice-configurable: looked up right away; naming a home by voice switches `homeAuto` off. Details: [docs/features/clock.md](docs/features/clock.md)
- **Find home through zitro.spot** (`homeAuto`, default off: a child's location stays with the parents unless they choose): when on, the stick asks `zitro.spot/api/locate.php` where its internet address is (DB-IP Lite, roughly the town; wrong with a VPN or mobile data) and that place is home, ahead of `homeCity`. Asked when online and idle, again after 12 hours or on another WiFi network (`homeFound`, `homeFoundNet`); a failed request waits 10 minutes. The setup page shows the place found and credits "IP Geolocation by DB-IP"; Zitro's code on the server stores nothing. Requested by the user on 2026-11-15

### Audio Settings
- **Speaker Volume**: 0-255 (default: 210)
  - Applied during speaker initialization in setup() (read from Preferences)

### Branding
- Boot splash shows 2 s on power-on and restart, 1 s after a wake from deep sleep (no 1 s serial wait before it then either). The WiFi search runs meanwhile
- Boot splash: "zitro.spot" wordmark only, on black (240x135 PNG in `logo_data.h`), generated by `scripts/make_zitro_brand.py`; the wordmark keeps at least 24 px padding left and right and is centered vertically
- Development builds only: two small grey lines under the wordmark name the build, so sessions sharing a stick can tell whose firmware runs. First line, up to 4 feature keywords, newest first (the current branch, then merged branches, one word each: `feature/vocab-trainer` → `vocab`); second line, `#<commit>` (5+ hex digits, `+` when the sketch folder had uncommitted changes) and the build time, e.g. `build, fast, sticks3, voci` / `#9b0a4+, 18:23`. The same goes to serial as `Dev build: … | …`. `scripts/zitro_build_info.sh` makes it inside every arduino-cli compile on the Mac, through a hook in the esp32 core (`platform.local.txt` plus `zitro_build_hook.sh`) that `scripts/install_build_hook.sh` installs (verify.sh and deploy.sh run it every time). The hook passes paths as arguments, never as shell code, and acts only for sketches inside a worktree of this repository, so other sketches on the Mac can't use it to run code. Release builds set `ZITRO_RELEASE=1` and show the wordmark alone; `installer/rebuild_installer.sh` stops if build info got in anyway
- Boot jingle: two short organic pops, the second one higher (about 0.6 s, at most 1.9 s, 16-bit PCM mono 16kHz, `splash_jingle_data.h`), start with the boot splash and end before it does, so boot takes no longer. Plays on power-on and restart, not on a wake from deep sleep. Follows the Speaker Volume setting. Made with ElevenLabs by `scripts/make_splash_jingle.py` (key: `ELEVENLABS_API_KEY`, from the environment or the gitignored `.env`; only needed to make a new jingle, the stick never contacts ElevenLabs)
- Setup hotspot (open WiFi) is named "zitro"; mDNS makes the setup page reachable at `zitro.local`. The WiFi QR code (`WIFI:T:nopass;S:zitro;P:;;`, QR version 3, 93x93 px) is made by `generate_wifi_qr.py` and `convert_wifi_qr.sh`
- Setup page follows zitro.spot (webpage2/styles.css): paper palette, Sniglet headings, lemon-shaped citrus buttons. Sniglet is served from flash (`setup_fonts.h`, `/f/sniglet-400.woff2`, `/f/sniglet-800.woff2`, made by `scripts/make_setup_fonts.py`) because setup mode has no internet
- Home screen (`drawHomeScreen()`, drawn whenever `MAIN_QUESTION` is shown): "[lemon] Press [M5] button" on one line: small citrus lemon (#f2b705, with leaf), then "Press", a keycap labeled "M5" like the stick's front button (rounded dark grey face with white outline, smaller DejaVu 12 text, 2 px mid-grey key side showing bottom and right), then "button"; "to ask a question"; half-line gap; shake icon (`shakeIcon`, phone slightly tilted) + "Shake for a surprise."
  - One click of Button B there opens the home menu (see Requirements for the Home Menu)
  - On the StickS3 the key looks like its front button (`drawFrontKeycap()`): an upright blue pill with no label, face 7 px wide and as tall as the M5 key's face, corner radius 3, fill `0x3C7F`, the same 2 px mid-grey side bottom and right. The tour, the vocabulary trainer and the shop draw their keys with it too, so the StickS3 never shows "M5"
  - In German (Settings → language DE, the default): "[lemon] Drück die [M5] Taste", "und stell eine Frage", shake icon + "Schüttle mich!" ("Schüttle für eine Überraschung." is 281 px wide, the screen 240)
- WiFi search icon (see WiFi in the Background): three arcs over a dot, 17x14 px, drawn with primitives, top right at (WIDTH-21, 3), left of the battery indicator when that shows. Only on the home screen, the recording screen, and the screens that wait for WiFi while showing something else ("Restoring...", "Restoring image..."). Pulses grey (90) to white and back every 1.4 s while searching, stays grey once a search found nothing. When the connection stands it turns into the connected icon
- WiFi connected icon (requested by the user on 2026-11-16: "show a quick wifi material icon with a check tick and then fade it out"): Material Symbols has no WiFi-with-tick icon, so it puts two of its Rounded icons (weight 400) together the way Material badges an icon: "wifi" in white and a filled "check_circle" in green (#00e676, Material green A400) in its bottom right corner, a 1 px black ring cut around the badge. 23x18 px, anti-aliased, generated by `scripts/make_wifi_done_icon.py` into `wifi_done_icon.h`. Its box is 2 px further right and 1 px higher than the searching icon's and covers it. Takes the searching icon's place the moment the connection stands, stays at full brightness for 0.6 s, then fades to black over 0.5 s (smoothstep, `wifiDoneLevel()` in `wifi_core.h`, tested). Only when the searching icon was on the screen at that moment: not when the connection comes during the logo, on another screen, with the display asleep, or while something waits for WiFi (the WiFi page, "Restoring..."), because what comes next is drawn right away there. A full redraw of the home or recording screen during the fade draws it at its current brightness; any other screen ends it
- WiFi page (`wifi_link.ino`, requested by the user on 2026-11-15: the WiFi screens with a pulsing WiFi symbol and a progress bar): full screen, black, landscape, while something can't go on without WiFi (a recorded question, the setup screen's retry). A large WiFi symbol in the corner icon's shape (dot radius 4, arcs 10-14, 19-23 and 28-32 px, 225° to 315°), dot centre at (120, 48); a white title (10 pt) centred at y 76; a grey (160) line at y 98, cut with "..." to fit; a rounded progress bar, 140x6 px at (50, 117), track grey (55), fill citrus (#f2b705). German or English after the Language setting. Three looks:
  - Searching: "Suche WLAN..." / "Looking for WiFi...". The line names the network being joined, followed by "2/3" when the scan found several (the count stays when a long name is cut), or "Netze in der Nähe" / "Networks nearby" during the scan. The dot and arcs pulse grey (90) to white like the corner icon, each arc 200 ms after the one inside it, so the light runs outwards. The bar moves on in steps, because how long a search takes isn't known ahead (`wifiProgress()` in `wifi_core.h`, tested): fast join 0-25 %, scan 25-45 % (0-30 % without a fast join), then an equal slice of the rest for each network found. Within a step it creeps towards the step's end without reaching it (half of it after 1 s fast join, 1.6 s scan, 2.5 s join), it never goes back, and it is full once connected. Every 40 ms only the symbol and the bar are drawn again, the whole page when the line changes
  - Connected: "Verbunden!" / "Connected!", the network's name, white symbol, full bar
  - Nothing found: "Kein bekanntes WLAN" / "No known WiFi found", "Öffne die Einrichtung..." / "Opening setup...", grey and still, no bar
- Setup screen: "SETUP: Connect to:" / bold citrus (#f2b705) zitro / "and open:" / bold underlined citrus "zitro.local" / "Then press BUTTON" "to retry." (retry lines only with stored networks), WiFi QR code on the right. Opens at boot without stored networks or AI key, on request, or when a recorded question finds no known WiFi
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
  - On the StickS3, B's tab sits lower and is as long as the key, and C's tab isn't drawn (see "Requirements for Supported Boards")
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

## Installer (macOS)

- One installer: `installer/Install-Zitro-<version>.dmg`, published at `https://zitro.spot/downloads/`. Signed with Developer ID, hardened runtime, notarized and stapled. macOS 11+, universal. There is no Windows installer
- `installer/rebuild_installer.sh` always compiles from scratch and copies nothing from earlier builds. Build output is not tracked in git
- A release must pass: parts match the merged image at their offsets, partition layout as expected, no part overlaps NVS, credential scan of the firmware and of every file in the final DMG, flasher tests (`native_flasher/tests/run_tests.py`), signature and notarization checks
- The credential scan lets one known item through and warns on every run: the Wolfram AppID from `local_secrets.h`. This is an open item, not a decision to keep it
- Before writing, the flasher verifies the package against `manifest.json` (SHA-256), then that the chip is an ESP32 and the flash at least 8 MB. It has no chip erase and can't write arbitrary files
- Writes are compressed at 460800 baud (921600 is unreliable on this board), with one retry at 115200. The chip verifies each region by MD5 from its flash
- Stored WiFi networks are ordered by last successful connection. With a full list (5), adding a network replaces the last one, on the setup page and from the installer
- Updates keep NVS (`0x9000`-`0xDFFF`): settings, keys and WiFi networks stay. The first SPIFFS sector (`0x670000`) is wiped: installers before 1.1 left credentials there in plain text
- AI service and WiFi are optional in the wizard. Sent over USB after the restart (`zitro-ping`, `provision <JSON>`, see `docs/CONFIG_HANDOVER.md`), validated by the same functions as the setup page, all or nothing, then the stick restarts. Failing to store settings doesn't count as a failed firmware install; the app says so and offers another try
- Secrets reach the flasher through a pipe only: never in arguments, files or logs. No serial command reads a key back. Device serial output isn't shown in the app, because it names WiFi networks

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
- Status and error screens follow Settings → language (DE, the default, or EN). Every fixed text goes through `uiText(de, en)`; the vocabulary trainer uses its own `vt()`. German texts, each checked to fit its screen:
  - Transcribing... → "Ich verstehe...", Thinking... → "Ich überlege...", Calculating... → "Ich rechne...", Creating image... → "Ich male...", Finding related image... → "Ich suche ein Bild...", Looking it up... → "Ich schlage nach..."
  - Restarting... (recording) → "Nochmal von vorn...", Restoring image... → "Hole das Bild...", Restoring... (3D model) → "Hole das Modell...", Resetting... → "Setze zurück..."
  - Mic error → "Mikrofehler", "No known WiFi found." → "Kein bekanntes WLAN gefunden.", "Couldn't hear. Try again." → "Nicht verstanden. Versuch es nochmal.", "Retrying WiFi connection..." → "Verbinde das WLAN nochmal...", "Connected!" → "Verbunden!", the welcome screen without an AI key, "Not enough memory for the 3D view." and the image errors
  - Printing: "Ich suche...", "Drucke...", "Gesendet!", "Drucken ging nicht", "Kein Drucker gefunden", "Kein Bild"
  - Texts that were German only now have English: "No timers running.", "All timers cleared.", "Sleeping z z z"
  - Not yet: the setup screen (built-in font 2 has no umlauts), the image decode errors, and the error hints with "Triple-press the side button" (the button UI module owns that wording)

### Language-Specific Prompt Handling
- German (DE): No prompt modification (system default)
- English (EN): Appends " Answer in English." to user queries
- The device introduces itself as "Zitro" in the system prompt

### Worries and sadness (main prompt, `care_core.h`)
- When a child shares worries, sadness, fear, loneliness, conflict, exclusion, bullying, pain, or that someone hurts it, Zitro answers briefly, warmly and seriously, without a diagnosis, without claiming feelings or friendship, and sends it to a person it trusts (parents, teacher, school social worker; with pain, parents or a doctor). This follows the Zurich KI-Playbook's advice on "KI als Freund" and "KI als Coach"
- If it sounds serious (someone hurts the child, great fear, self-harm, not wanting to live), Zitro names the free children's helpline of the home city's country and, in acute danger, 112. The number comes from a table in the firmware, chosen by the home city's time zone (a home found through zitro.spot counts too, so a VPN abroad can change it), never from the AI's memory: Switzerland `Europe/Zurich` 147 (Pro Juventute), Austria `Europe/Vienna` 147 (Rat auf Draht), Germany `Europe/Berlin` 116 111 (Nummer gegen Kummer). Elsewhere no number: get an adult right away
- Such answers name no other phone numbers and show no picture (`image_query` empty, `create_image` false). Ordinary questions about emergency numbers are answered as before
- Host tests: `tests/care/run.sh`. Checked on 2026-11-15 against `gpt-6-luna` with the real prompt: worries get a trusted person and, when serious, 147 and 112; "Bist du mein Freund?" no longer gets "Ja"

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
6. **Fallback**: On error, retry with `gpt-image-2.6-flare` (`gpt-image-1.5` shuts down on 2026-12-01; dall-e-2 was removed from the OpenAI API in May 2026)

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
   - No new WiFi search (after a lost connection or a search that found nothing); one under way finishes
   - CPU remains active but idle

2. **Light Sleep** (90 seconds of inactivity)
   - WiFi powered off
   - CPU paused
   - RAM retained
   - Instant wake-up on button press: the previous screen is back at once, WiFi is searched in the background

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
3. Display turns on immediately (light sleep) or boots up (deep sleep, 1 s logo, then the restored answer or the home screen); WiFi search starts in the background
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
- No new WiFi search starts (`wifiTick()` gets `awake = false`)

This prevents unnecessary CPU usage and IMU sensor reads during sleep states.

## WiFi in the Background

Requested by the user on 2026-11-14: start faster after a wake, record right away, find WiFi meanwhile.

- `wifi_link.ino` owns the radio while not in setup mode; nothing else calls `WiFi.begin` or blocks waiting for a connection. The state machine and the join order are pure logic in `wifi_core.h`, tested by `tests/wifi/run.sh`
- The radio joins one network at a time, so all networks are seen at once by one scan instead of trying the stored ones one after another
- Order: (1) fast join with the channel and router of the last connection (kept in RTC memory through light and deep sleep, matched to the stored networks by name), up to 3 s; (2) one active scan of all channels, 120 ms per channel (about 1.6 s, at most 5 s), hidden networks included; (3) join the stored networks the scan saw, strongest first (stored order on a tie), each up to 8 s with the scanned channel and router. A stored network the scan didn't see is tried by name only when the scan saw a hidden network. A join that reports a failure (any disconnect reason but our own) moves on once it has run 1 s
- Nothing found: search again every 30 s while the screen is on. A lost connection: search again (fast join first) while the screen is on
- The search starts while the boot logo shows (after the jingle, as the StickS3 can restart on battery with speaker and radio both drawing) and on every wake from light sleep. Setup no longer waits for it: the home screen, the restored answer or the tour come at once
- On connection: the network moves to the front of the stored list, RTC memory remembers it, NTP starts, and the corner icon shows the connected icon for about a second (see Branding, "WiFi connected icon")
- A recorded question waits for the connection on the WiFi page (see Branding), up to 25 s or until a whole search found no known network. The page comes after 300 ms of waiting, so a connection that is nearly there doesn't flash it; "Transcribing..." follows once connected. Nothing found: the page's "nothing found" look for 2 s and the setup screen (hotspot), to add the network. Setup opens on its own only then, or at boot without stored networks, or without an AI key
- Restoring a picture or 3D model after deep sleep waits the same way on "Restoring..."; the answer text shows at once
- Requests that find no connection (`reconnectWiFiNow()`) wait for the search, up to 30 s, without changing the screen; a request that failed on a connection joins again first. Speech to text checks the connection too (vocabulary trainer, shop game)
- Setup screen retry (Button A): the WiFi page at once, then the same search, blocking, up to 30 s. Connected: the page's "connected" look for 2 s, then the home screen. Not connected: back to the setup screen

---

# Requirements for Get Zitro and the Shop

- zitro.spot names both sticks: every page's footer says "Runs on the M5StickC PLUS2 and the M5Stack StickS3." (`.footer-devices`, its own text block under the tagline, so the footer columns keep their translations)
- "Get Zitro" asks "Do you have an M5StickC PLUS2 or an M5Stack StickS3?" with three buttons, in this order: "Yes, S3", "Yes, PLUS2", "Not yet". Both yes buttons open the e-mail form for the installer link; "Not yet" opens the shop. No device photo, as there is none of the StickS3 yet. "Get Zitro" on the other pages links to `index.html?download=true`, which opens the same question (it used to skip it and show the e-mail form)
- The download request sends `hasDevice` as `s3` or `plus2` (`send-download.php`). Pages cached from before still send `yes`, which is kept. Anything else is stored as `unknown`. The notification e-mail to the admin names the stick in the subject and body ("New Download Request (M5Stack StickS3)", "Stick: M5Stack StickS3"), because StickS3 owners get the separate StickS3 installer
- The shop sells Zitro pre-flashed on either stick, at the same price: "Your stick" comes first in the order form: two radio choices (`name="device"`, PLUS2 preselected), each with a circle in the stick's case colour and the colour's name: PLUS2 yellow (`#f5c400`), StickS3 gray (`#9b9a97`). shop.js sends `device` (`plus2` or `s3`). `submit-order.php` refuses other values and treats a missing one as `plus2` (shop pages cached from before sold only the PLUS2). The order is stored with its stick (`orders.device`; `db.php` adds the column to older databases, older orders count as `plus2`), and the notification e-mail names it in the subject and body ("New Order #12 - M5Stack StickS3 - $55.00", "Stick: M5Stack StickS3")
- `deviceName()` in `db.php` turns `plus2`/`s3` into the full names; the admin page shows the stick for download requests and orders

# Requirements for Website Languages

- zitro.spot (`webpage2/`) speaks English, German, Europe's languages and the 7 most spoken languages worldwide: the 24 official EU languages, Romansh, Norwegian, Icelandic, Bosnian, Serbian (Latin script), Macedonian, Albanian, Turkish, Ukrainian, Belarusian, Russian, Luxembourgish, plus Chinese (simplified), Hindi, Arabic (right to left) and Bengali. The list lives in `scripts/site_i18n.py` (`LANGUAGES`)
- Pages translated: index, learn, detox, create, compare, shop. The onboarding page and the word lists page (`/voci`) stay German and English like the stick
- The English text stays in the HTML. Every text block that holds no other block and no element with an `id` gets `data-i18n="<key>"`; `alt`, `placeholder`, `aria-label`, `title` and the description/title meta tags get `data-i18n-a="<attr>:<key>"`. The key is FNV-1a (32 bit, over UTF-16) of the block's visible text plus its link targets, so a block whose text or links change gets a new key
- `webpage2/i18n/en.json` lists every key with its English HTML, and the strings scripts show through `T('…')` / `window.zitroT('…')` (keyed by the English string, `{placeholders}` kept). `webpage2/i18n/<lang>.json` holds `{"lang", "dir", "t": {key: html}, "js": {english: text}}`
- Elements that scripts hold (an `id` inside) are never replaced as a whole: their parts are translated one by one, so script.js and shop.js keep working on the live elements
- `site.js` chooses the language: `?lang=xx`, then the saved choice (`localStorage` `zitroLang`), then the browser's languages (`no`/`nn` count as `nb`), else English. It downloads only that file (`cache: no-cache`), swaps the blocks and attributes, and sets `lang` and `dir` on `<html>`. Before swapping a block it recomputes the key from the live page; if it doesn't match (the page changed after the last extract), the block stays English. A small script in each page's head hides the page (class `i18n-wait`) until the translation is in, at most 2 s
- Language picker: a globe with the language code in the header (globe only on phones), a native list on top, names in their own language. It offers only languages that have a file; `extract` writes that list into `site.js`
- `python3 scripts/site_i18n.py extract` sets the keys and writes en.json and the language list. `check` fails when keys are out of date, a translation has an unknown key, its markup (tags, href, class, id, target, rel) differs from the English, it contains script, or a script string loses a placeholder; it prints how much of each language is translated. `todo <lang>` prints what a language lacks
- Translations other than German were made by AI and haven't been reviewed by native speakers yet

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

- **Music on an endless band**: [docs/features/music.md](docs/features/music.md). Songs are looked up by title: built-in table first, then the server index. The rules for server songs, the fixed "don't know" sentences and the memory limit are in that file
- **Function graphs**: [docs/features/graph.md](docs/features/graph.md)
- **Recipes step by step**: [docs/features/recipe.md](docs/features/recipe.md)
- **Globe with a pin**: [docs/features/globe.md](docs/features/globe.md)
- **World clock and day and night**: [docs/features/clock.md](docs/features/clock.md). The written answer to a time question is always computed by Zitro, never the AI's. Each zone is its POSIX rule plus, where the tz database departs from it within 6 years, fixed offsets before it (`until`, e.g. Cuba's winter 2026/27), checked against the tz database by `tests/clock/run.sh`; `scripts/update_zone_rules.py` brings both tables up to date with a new tz release. 320 cities are built in and work offline; other places come from the zitro.spot server (`webpage2/api/place.php`, GeoNames). `webpage2/api/locate.php` says roughly where the asking IPv4 address is (DB-IP Lite mapped to GeoNames places, `scripts/make_ip_data.py`), for the home setting a parent can switch on; the script stores and logs nothing, and the data (37 MB, monthly) is uploaded, not committed
- **ISS live**: [docs/features/iss.md](docs/features/iss.md). The zitro.spot server (`webpage2/api/iss.php`) turns NASA's ephemeris into a track; the written answer is computed, never the AI's. With `?places=1` it also names the place under the track from Natural Earth (`webpage2/data/iss`, made by `scripts/make_iss_places.py`); disputed regions inside a country stay blank
- **Proteins, DNA and crystals**: [docs/features/protein.md](docs/features/protein.md)
- **Night sky**: [docs/features/sky.md](docs/features/sky.md)
- **Physics scenes and geometric solids**: [docs/features/physics.md](docs/features/physics.md)
- **Timelines with pictures**: [docs/features/timeline.md](docs/features/timeline.md)
- **Dinosaur fact sheets** (picture, facts, size next to a 1.70 m person, fun fact): [docs/features/dino.md](docs/features/dino.md)
- **Organs, skeleton and cells**: [docs/features/anatomy.md](docs/features/anatomy.md)
- **Wolfram results**: [docs/features/wolfram.md](docs/features/wolfram.md)

## Plug-ins (`edu_feature.h`)
- Each kind fills in one `EduFeature` in `feature_<kind>.ino` with pure logic in `<kind>_core.h`, tested by `tests/<kind>/run.sh`; all are registered in `feature_registry.ino`. The AI prompt collects each feature's `promptRule`
- `model_view.ino` owns the sprite, the gyro, the sleep rules and the bottom line; `fetchModel()` in `model_source.ino` dispatches by kind
- Prepared data lives in `webpage2/data/<kind>/` and is read from `https://zitro.spot/data/<kind>/`. It's static hosting, uploaded separately from the firmware. Without it those kinds fall back to the picture. For songs, `webpage2/data/music/` holds the server songs and their `SOURCES.txt`; the 26 built-in songs need no server data and play without WiFi
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
- The globe views (globe, world clock, ISS: `EduFeature.levels`) stay level inside that locked rotation. The globe sits in the middle of the screen at a fixed size (`GLOBE_LEVEL_RADIUS`, between the top and the bottom line) and is drawn turned by the stick's roll, so turning the stick in its own plane, or holding it upright, leaves the globe exactly where it is with north up. Gravity in the screen's plane alone sets the roll, ignoring readings off 1 g by more than 0.35 g (shakes); lying flat (under 0.35 g in the plane) the roll holds. The gyro isn't used for it: an offset like the Plus2's start-up zero would turn the picture by itself. Tilt and swivel turn the globe as before, in the upright picture's axes; the in-plane turn no longer spins it. The text, the pin, the markers, the bottom line and the button tabs snap to the nearest of the four ways to read (the sprite's own rotation) once the stick is 55 degrees past the last one; the tabs stay next to the physical buttons. Pure logic in `levelStep`/`levelMatrix`/`levelToReading` (`model_core.h`, `tests/model/run.sh`); serial `roll <degrees>` / `roll off` forces the roll for screenshots. During the tour the text stays in landscape. Requested by the user on 2026-11-15
- The globe views draw the home city as a small white house with a black rim, upright for the viewer, under the pin; only on the near side and never as an arrow. Requested by the user on 2026-11-15. Where it would sit on the pin's or the station's tip (within `GLOBE_HOME_SIZE / 2 + 2` px each way, some 900 km at the first zoom) it is left out, so it doesn't blur where the pin points (found in the stick test on 2026-11-16; not yet confirmed by the user)
- Text in the globe views (world clock, ISS) has a 1 px black rim and no box behind it: boxes cut notches out of the centred globe and wiped the top of the line below (stick test 2026-11-16). In landscape the ISS's top right uses the whole width beside "ISS", so "422 km hoch" fits
- One non-blocking frame per `loop()`, at most 25 per second. Measured on the Plus2 at full speed: 50 atoms 58-62 fps, 100 atoms 51-55 fps, 200 atoms 42-46 fps
- Gyro integrated, 2x gain. It turns by itself at 0.35 rad/s only once the hand has stayed under 3 deg/s for 3 s (`modelIdleTurnAllowed`, `button_rules.h`), so what the hand turned to stays in view. The anatomy cutaway's rocking waits the same way (`modelIdleTurning`). Opening the view counts as long ago: lying still, it turns at once. Requested by the user on 2026-11-07
- Button tabs in every result view, like the print menu's: for 3 s when the view opens (also after Button C back from the text and after a wake) and for 3 s after each Button B or C press, then gone so the content has the whole screen. Button A: microphone (new question), filled. B and C: the feature's `hints()`; without one, a magnifier for zoom (no `buttonB`) and text lines for the written answer (no `buttonC`). Paging views (recipe, timeline, Wolfram) show ▲ back, none on the first page, and ▼ next, or text lines on the last page. Anatomy shows a half-filled circle while Button C leads to the other view. Physics scenes show ▼ for Button B when the scene has more than one setting. Music: pause or play, and the volume speaker. Tabs cover the content for those 3 s, the bottom line included. Hold and double-click actions have no tab. A view that keeps its picture (Wolfram) redraws once when the tabs go (`modelViewRedraw`). Requested by the user on 2026-11-07
- The sprite (internal RAM, PSRAM as fallback) lives only while the view is on screen: freed for questions, timer alerts and setup. No memory: a short note, then the text
- Atoms in PubChem colours (`atom_table.h`, made by `scripts/make_atom_table.py`), dark rim for depth, no highlight on small atoms because it reads as a + sign. Footer: label, source, element letters in colour; a smaller ASCII font unless the label needs the full font. The label is shortened with "..." to fit; when not one letter of it fits (a turned globe view with an umlaut name), the footer shows the source alone, in the small font if it can (`eduFooterFit`, `model_core.h`)

## Sleep
- Motion from 15 deg/s counts as activity, for up to 5 minutes after the last button press, entry or wake. Measured gyro noise on a desk: peak 0.62 deg/s. Motion never wakes a sleeping display
- Deep sleep saves the reference (`modelRef`) and fetches the model again on wake; on failure, text

## Tests
- `tests/model/run.sh`: reference parsing, SDF parsing with real PubChem files and broken input, rotation, depth order, colours
- Serial commands: `model <kind:query>`, `ask <question>` (typed question instead of the mic), `forget` (clear conversation memory), `pressb`, `pressbb` (double click), `pressc`, `holdc` (the view's button actions), `speaker` (volume and whether a tone is sounding), `modelbench <atoms>`, `modelgyro`, `modelhost <host>`, `modelnomem 1`

---

# Requirements for the Home Menu

Requested by the user on 2026-11-15: "Beim Hauptscreen der Button B soll das Menü zeigen, mit Settings, aber auch mit 10+ Fragen, die die Multimedia-Sachen auslösen", then the functions "direkt ins Menü nach B", "Timer anzeigen" prominent, and a line above the questions instead of a sub-list. Spec: `docs/superpowers/specs/2026-11-15-home-menu-design.md`.

- **Opening:** one click of Button B on any `SHOW_TEXT` screen (`buttonBClicked()`, both sticks): the home screen and every written answer, where B had no job. Or serial `home`.
  - Not the home screen alone, because `response` keeps the last answer: after one answer, the home screen can't be told apart from it.
  - Except on "Keine Timer aktiv." (`noTimersShown()`): a passing note, where B goes back to the home screen (`goHomeScreen()`: no answer kept). The note also drops the last answer's picture and 3D view, so no rotate or cube hint shows on it. User, 2026-11-16
  - The button module (`feature/button-ui`) will open it through `homeMenuOpen()` with its own press reader.
- **Code:** `home_menu.ino`. Rows, questions and presses are pure in `home_menu_core.h`, with host tests in `tests/home_menu/run.sh`. While it is open, `loop()` hands over to `homeMenuTick()` after the shop block, as for the trainer.
- **Rows:**
  1. Timer anzeigen / Show timers: `showTimerList(false)`.
  2. Tour starten / Start the tour: `guideStartNow()`.
  3. Überraschung / Surprise: `showSurprise()`, also when shaking is off.
  4. Wörter üben / Practise words: `vocabStart("practice")`, after the WiFi wait.
  5. Einstellungen / Settings: `openSettings()`.
  6. Menü schliessen / Close menu.
  - A grey line (0x7BEF) above row 7.
  - Rows 7–22: the 16 example questions (`HOME_QUESTIONS`).
  - Row 1 is preselected.
- **Look:** as `vocabDrawMenu`.
  - Landscape, 3 rows at y 14 + k × 36.
  - The selected row is a white rounded rectangle 204 × 30 at x 20, with black ink.
  - Each row has its own 20 × 20 icon at x 26 (`HOME_ROW_ICONS`, `home_menu_icons.h`), drawn in the row's ink: a timer, a flag (tour), a parcel (surprise), 文A (words), gears (settings), a cross (close); for the questions a star, a satellite, a rocket, a moon, a camera, a T. rex, a heart, DNA, a crystal, a drop, an icosahedron, a parabola, mountains, a clock, a pan and a note. Material Symbols Rounded (Apache 2.0), the T. rex, the parabola and the icosahedron drawn in the same grid; made by `scripts/make_home_menu_icons.py`. Requested by the user on 2026-11-16 ("Icons ... für Settings (Zahnräder), Timer (Timer-Uhr), Tour, Überraschung (Päckli). Stretch goal: für Orion, etc. auch")
  - Label at x 50 in `getFont12pt()` (Roboto 10 pt), at most 174 px wide (checked by the host test).
  - Scroll bar at x 230.
  - Tabs ✓ (A) and ▼ (B); no C tab.
- **Presses:**
  - Read by `menuPressRead(M5.BtnA.wasClicked(), M5.BtnB.wasClicked(), M5.BtnB.wasHold())` (pure, `home_menu_core.h`), on both sticks. The user, 2026-11-16: "in the menu double button b shouldn't have it's own function".
  - B click: next row at once, wrapping. No double-click window to wait for: two quick clicks are two rows.
  - Closing: holding B, or "Menü schliessen". Button C isn't read.
  - `forgetButtonBClicks()` after every B press, so the menu's clicks never come back to `loop()` as a single, double or triple press (the setup page) once it has closed.
  - A hold that closes the menu sets `buttonBHeldForMenu`. Until B is released, `loop()`'s 3-second WiFi reset doesn't count that press.
  - A: choose. It is read as `M5.BtnA.wasClicked()`, with `discardButtonA(0)` before and after.
- **Questions:**

  | # | DE | EN | Opens |
  |---|---|---|---|
  | 7 | Wo ist Orion? | Where is Orion? | `sky:Orion` |
  | 8 | Wo ist die ISS? | Where is the ISS? | `iss:now` |
  | 9 | Mondlandungen | Moon landings | `timeline:moon_missions` |
  | 10 | Mondphasen | Moon phases | `physics:moon` |
  | 11 | Foto: Marie Curie | Photo: Marie Curie | Wikipedia picture |
  | 12 | T-Rex | T. rex | `dino:Tyrannosaurus` |
  | 13 | Das Herz | The heart | `anatomy:heart` |
  | 14 | DNA in 3D | DNA in 3D | `protein:dna` |
  | 15 | Salzkristall | Salt crystal | `crystal:salt` |
  | 16 | Wassermolekül | Water molecule | `molecule:water` |
  | 17 | Ikosaeder | Icosahedron | `solid:icosahedron` |
  | 18 | Kurve von x² | Graph of x² | `graph:x^2` |
  | 19 | Matterhorn | The Matterhorn | `globe:Matterhorn` |
  | 20 | Uhrzeit in Tokio | Time in Tokyo | `clock:Tokyo` |
  | 21 | Pfannkuchen | Pancakes | `recipe:pancakes` |
  | 22 | Twinkle Twinkle | Twinkle Twinkle | `music:twinkle-twinkle` |

  - Every question has a fixed answer: a label, a short line in DE and EN without facts ("Hier ist Orion."), its reference, and an `imageQuery`. The `imageQuery` is the photo itself, or the fallback picture when the view can't open.
  - Choosing one builds a `GPTResponse` and calls `showAnswer()`, the function a spoken question ends in, moved out of `handleVoiceQuery()`. So view, text, tabs, printing and memory work the same; the label goes into the memory as the question.
  - Never an AI call, read-aloud, Wolfram or an AI picture.
- **WiFi:** the questions and the trainer wait for it as a spoken question does (`wifiWaitForLink(WIFI_QUESTION_WAIT_MS, WIFI_SHOW_PAGE)`). With no known network, the WiFi page, then setup.
- **Night lock** (on, 8 pm to 6 am): the tour, the surprise, the trainer and the questions show "Ich schlafe z z z" and switch the display off, as a spoken question does. The timers, the settings and closing still work.
  - Both use `showNightLockAndSleep()`, which also sets `displaySleeping` and `POWER_DISPLAY_SLEEP`. Before, the panel went dark while the stick still counted as awake, so a press acted on the dark screen.
- **Sleep:** as the trainer.
  - A press while the screen is dark or in the wake cooldown only wakes it and redraws.
  - The menu keeps the stick awake for 5 minutes after the last press.
  - Deep sleep closes it (`saveStateToPreferences()`).
  - A timer alert comes on top; dismissing it redraws the menu.
- **Serial:**
  - `home`: open the menu.
  - `home <row>`: choose row 1–22 directly.
  - `homekey a|b|c`: a press in the open menu (b = next, c = close).
- **Moved out for it, with no change in behaviour:**
  - `showTimerList(bool playAudio)`, from the voice path. "Keine Timer aktiv." / "No timers running." now follows the language.
  - `showSurprise()`, from the shake block. It returns true where the block used to end the loop pass.
  - `openSettings()`, from the triple press.
- **Example pictures:** `imageQuery` is an English Wikipedia title, because `searchWikipedia` asks en.wikipedia.org's summary for that exact title. All were checked live on 2026-11-15 and have a picture, except "Twinkle, Twinkle, Little Star". The song is built in and doesn't need one.

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
- The card lives only as long as its timer: once that timer is deleted or has fired, the card is forgotten and redraws show the home screen instead

## Timer Alert Behavior

- When a timer fires: wake display, show bell icon (3x `bellIcon`) + label (14 pt, up to 2 lines; longer labels 10 pt, up to 3 lines), play alert sound
- The alert screen blinks: full white screen with black content and full black screen with white content swap every 500 ms until dismissed; blinking stops while the display sleeps
- The bell swings with the blink: turned -10° (white frame) and +10° (black frame) around its hanger (`drawIconRotated()`)
- Portrait: bell above the label (14 pt, up to 3 lines; longer labels 10 pt, up to 4 lines), clear of the A tab
- ✓ tab next to Button A (same edge tabs as the print screens); no instruction text
- Alert sound: embedded WAV (16-bit PCM mono 16kHz, ~0.93 seconds), repeated with a 0.3 s pause until dismissed. Non-blocking, so the blink and Button A keep working; Button A cuts the sound off at once
- The sound keeps going while the display sleeps and ends when the stick enters light sleep (default 90 s without a button press), because light sleep switches the speaker off. Waking the stick with the alert still open starts it again
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

---

# Requirements for the Guided Tour

Design: `docs/superpowers/specs/2026-11-08-guided-tour-design.md`. Logic in `guide_core.h` (tests: `tests/guide/run.sh`), drawing in `guide.ino` and `guide_tiles.ino`.

## Start and end

- Starts at the end of `setup()` when Preferences `settings/tourDone` is false, it isn't a wake from deep sleep, AI and voice keys are configured and AP mode isn't running. Updates keep the flag unset, so existing sticks see the tour once
- The AI sets `is_tour` for "Was kannst du?", "Zeig mir die Tour", "What can you do?", "Show me the tour": the tour starts whatever `tourDone` says
- Finishing or "End tour" sets `tourDone` and clears all tour content. A blocked first question (parental lock, missing key, a request for setup mode) stops the tour without setting it

## Flow and buttons

- Ask (one real recording and AI answer) > Answer > Photo > Sky (`sky:Orion`) > Music (`music:twinkle-twinkle`) > Grid (4 pages, 2x2) > Done. Only the first question calls the AI
- Not heard or mic error: Retry screen. Cancelled: back to Ask
- During the tour, `loop()` hands over to `guideTick()` right after the timer blocks: no other button handler runs. Button C doesn't count towards opening the settings, holding Button B doesn't reset WiFi, and tabs stay on the screen
- C is next. Button A opens the menu, except on Ask, Retry and Done, where it records or starts the first normal question. Button B opens the menu everywhere
- Menu: Continue (preselected), One step back (not on Ask), End tour. A confirms, C moves and wraps, B closes
- One step back: grid page n to n-1, grid page 1 to Music, Music to Sky, Sky to Photo, Photo to Answer (to Retry without an answer), Answer and Retry to Ask, Done to the last grid page. It brings back the child's own result
- A quote screen comes before Photo, Sky and Music (1.9 s, C skips it, not when going back). Music stops while the menu is open
- Grid slides with C or tilting (timeline gesture), and the pages have no back button

## Sleep

- A press keeps the stick awake for 5 minutes (`GUIDE_KEEP_AWAKE_MS`), then the normal timers apply
- A press on a dark screen or during the wake cooldown only wakes and redraws the step. After deep sleep the tour is gone until the next power-on

## Content

- Moon picture: `guide_moon.h`, made by `scripts/make_guide_moon.sh` from `File:Moon nearside LRO.jpg` (Wikimedia Commons, public domain, NASA/GSFC/Arizona State University)
- Tile sentences use `DejaVu9` (ASCII only); the test checks titles and sentences against the real fonts
- No world wars in the tour's examples
- Serial for tests: `guide` starts the tour, `guidekey a|b|c` presses a button, `guidetype <text>` stands in for the next recording

---

# Requirements for the Vocabulary Trainer

Design: `docs/superpowers/specs/2026-11-13-vocab-trainer-design.md`. All requirements, the user guide part and the
spike results: [docs/features/vocab.md](docs/features/vocab.md).

## Decisions

- Quizzes on the display are wanted since 2026-11-13: the vocabulary trainer first, a shop role-play with the words
  ("Verkäuferlis") and quizzes on any topic later. This reverses "no quiz mode on the display" (2026-11-05).
- Zitro ships no word lists. Parents upload their own to zitro.spot/voci, tied to the stick by a code shown as a QR
  code. No account. (2026-11-14: this means school lists; the shop game brings its own 24 everyday goods.)

## What changes for existing behaviour

- In the trainer, Button A answers instead of asking a new question (like the tour); B opens the trainer's menu, C is
  "I don't know". The menu and the choice of books and lists (`vocabListActive()`) press as the home menu
  (`menuPressRead()`): each click of B is the next row, holding B goes back, A chooses, C isn't read; tabs ✓ and ▼
  (user, 2026-11-16). Holding B doesn't reset WiFi and three presses of C don't open the settings there.
- Spelling records at 16 kHz for up to 15 s and stops after a pause, whatever the recording settings say. Normal
  questions are unchanged.
- `transcribeAudio()` and `speakText()` gained parameterised versions (`transcribeSamples()` with language and prompt,
  `speakTextStyled()` with a speaking style); the old calls send exactly what they sent before.
- The answer JSON has a new field `vocab`; the prompt has one more rule.
- `eduHttpGetWithHeader()` sends one request header and accepts plain `http://` (for a test server).
- New button tab `HINT_DONTKNOW` ("?").

---

# Requirements for the Shop Game ("Verkäuferlis")

All requirements, the user guide part and the release note: [docs/features/shop.md](docs/features/shop.md).
Screens drawn by the stick's own drawing code: `screenshots/shop/`.

## Decisions

- 2026-11-14, the user: a first-person scene at the counter (lemonade bar, bakery, till), as the guest or as the
  seller, new guests keep coming and order. "ja loslegen" to building the game with its own drawn goods.
- No chat model in the game: the stick hears the order itself and answers from fixed sentences, so nothing is made up
  and answers come fast. Only speech-to-text and text-to-speech go out.
- The child serves by voice, not with buttons (Claude's call while building; the user had seen "buttons" proposed).

## What changes for existing behaviour

- In the game, Button A answers, B opens the game's menu, C is "?" (like the vocabulary trainer). Holding B doesn't
  reset WiFi and three presses of C don't open the settings there.
- The game's menu presses as the home menu (`menuPressRead()`): each click of B is the next row, holding B goes back,
  A chooses, C isn't read; tabs ✓ and ▼ (user, 2026-11-16)
- The answer JSON has a new field `shop`; the prompt has one more rule.
- `speakTextVoiced()` takes the voice; `speakTextStyled()` and `speakText()` still use "nova".
- The vocabulary trainer's recording loop moved to `record_speech.ino` (`recordSpeech()`), unchanged, with the screen
  drawn by the caller.
