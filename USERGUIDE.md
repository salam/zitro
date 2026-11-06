# Zitro (AIStick) Voice Assistant User Guide

## Choosing your AI

Zitro can answer with different AI providers. Pick one on the setup page under **AI**:

| Provider | What you need | Notes |
| --- | --- | --- |
| **Zitro subscription** | Zitro key (`zk_...`) | Coming soon, shown but not selectable yet. One key for answers, voice and pictures. |
| **OpenAI (ChatGPT)** | OpenAI API key | Default model `gpt-6-luna`; also `gpt-6.1-terra`, `gpt-6-astra`, `gpt-5.5`. |
| **Anthropic (Claude)** | Claude API key + OpenAI API key | Default model `claude-opus-5-5`; also `claude-sonnet-5-5`, `claude-haiku-5-5`. |
| **Infomaniak AI (Switzerland)** | API token (ai-tools scope), AI Tools product ID + OpenAI API key | Suggested model `qwen3`. Check the exact names your product offers in the Infomaniak Manager (or `GET /2/ai/{product}/openai/v1/models`). Voice and pictures still go to OpenAI. |
| **Other (OpenAI-compatible)** | Base URL (https), API key, model + OpenAI API key | For example Mistral, Groq or OpenRouter. |

- Voice input (speech-to-text), read-aloud and picture creation always run on OpenAI, or on the Zitro relay for subscribers. Claude, Infomaniak and most compatible providers can't hear or speak, so they need an OpenAI key as well. Without it, Zitro shows "Voice needs an OpenAI or Zitro key".
- Keys are stored only on the device. The setup page never shows them again, only the last 4 characters. Leave a key field empty to keep the stored key.
- Models: you can type any model name. The suggestions list the current models.

## Conversation memory

- By default Zitro remembers the last **10 minutes** of your conversation, so follow-up questions work ("Who built the Eiffel Tower?" then "How tall is it?").
- At most the last 5 questions and answers are remembered. Older ones drop out automatically.
- The memory lives only in the device's working memory. It survives sleep but is erased on power-off, and it is never saved to flash or sent anywhere except along with your next question to the AI you chose.
- Settings, timers and Wolfram calculations are not remembered.
- Turn it off on the setup page (**Settings > Remember the last 10 minutes**) or by voice ("Gespraechsgedaechtnis aus", "turn off conversation memory"). Turning it off, or changing the language, erases the memory immediately.


## Project Website

Visit the [Firebird landing page](webpage/index.html) for:
- Product overview and feature showcase
- Product comparison table vs other AI wearables (Ray-Ban Meta, Humane AI Pin, Rabbit R1, and more)
- Download links for installers (macOS and Windows)
- User testimonials and success stories
- Direct access to GitHub repository
- Information about the project creator

The website is located in the `/webpage` folder and can be deployed to any static hosting service.

## macOS Installer

For easy firmware installation on macOS, use the pre-built installer:

### Quick Install (End Users)

1. Download [AI-Stick-Flasher-1.0.dmg](installer/AI-Stick-Flasher-1.0.dmg)
2. Double-click to mount the disk image
3. Connect your M5StickC Plus2 via USB
4. Double-click "AI Stick Flasher"
5. Follow the on-screen prompts

The installer includes:

- Native macOS GUI with AppleScript dialogs
- Automatic device detection
- Pre-compiled firmware (no build tools required)
- Progress monitoring in Terminal
- Size: 1.0 MB (compressed DMG)

For detailed instructions and troubleshooting, see [installer/INSTALLER_GUIDE.md](installer/INSTALLER_GUIDE.md).

### Rebuilding the Installer (Developers)

After updating firmware, rebuild the installer:

```bash
cd installer
./rebuild_installer.sh
```

This will:

1. Compile the latest firmware
2. Update the app bundle
3. Rebuild the DMG
4. Verify integrity

The installer is dependency-free (except esptool) and uses native macOS dialogs.

## Development and Deployment

### Building and Deploying Firmware

The project includes deployment scripts for building and uploading firmware to your AI Stick device.

#### Quick Deploy

```bash
./deploy.sh           # Build, upload, and open serial monitor
```

#### Advanced Options

```bash
./deploy.sh --build   # Build only (verify compilation without uploading)
./deploy.sh --clean   # Clean previous build and rebuild from scratch
./deploy.sh --monitor # Open serial monitor only (no build/upload)
./deploy.sh -y        # Auto-confirm all prompts (useful for CI/CD)
./deploy.sh --port=/dev/ttyUSB0  # Specify custom serial port
```

#### Verify Compilation Only

```bash
./verify.sh           # Compile without uploading (quick syntax check)
```

**Requirements:**

- arduino-cli installed (`brew install arduino-cli` on macOS)
- ESP32 board support (installed automatically on first run)
- Device connected via USB

## Available interactions

### Buttons (M5StickC Plus 2)
- Button A (front "BUTTON")
  - Single click: ask a question, text answer only.
  - Double click: ask a question, text answer plus spoken audio.
  - Triple click: capture photo with camera and analyze image (if camera attached).
  - Sleep wake: first press shows previous result; press again after 2 seconds to start recording.
  - While recording: tap once to start the recording over; tap twice quickly to cancel and show the last answer again.
  - AP mode: retry WiFi connection.
  - Print menu: confirm the printer, then the page size. The last confirmation prints.
- Button B (side)
  - Short click (image view): scroll up in the image.
  - Hold about 1 second (image view): open printer discovery and selection.
  - Hold about 3 seconds (normal mode): reset WiFi credentials and restart.
  - Hold about 5 seconds (AP mode): reset WiFi credentials and restart.
  - Print menu: one step back; in the printer list, cancel.
  - Timer list: delete the shown timer.
- Button C / PWR (bottom)
  - Triple click: open settings/setup page (enters AP mode). Not in the printer list, timer list or a timer alert.
  - Short click (image view): scroll down in the image.
  - Short click (text view with image available): toggle text vs image view.
  - Print menu: next printer, page size or zoom value.
  - Timer list: show the next timer.

### Gestures
- Shake: triggers a surprise fact query (no voice playback).
- Rotate: rotate to the opposite orientation and back to toggle text vs image.
- Roll: toggles image zoom mode (contain vs cover) while image is shown.
- To save battery, the gyroscope (needed for roll) only runs while an image or equation is on screen. Shake and turning the stick still work everywhere.

### Camera (M5 Unit V / M12 Camera - optional)
- If attached, device auto-detects camera on startup.
- Triple-click Button A to capture photo and analyze.
- Camera analyzes objects, plants, and provides kid-friendly facts.
- Camera sleeps during light sleep (stage 2) and deep sleep (stage 3).

### Voice commands
- All device settings can be changed by speaking naturally.
- The device uses GPT to understand what you want to change. Examples:
  - "Lautstaerke auf 100" or "Set volume to 150"
  - "Switch to English" or "Sprache auf Englisch"
  - "Aufnahme 10 Sekunden" or "Recording time 15 seconds"
  - "Kindersicherung ein" or "Parental lock off"
  - "Bildersuche aus" or "Turn off image search"
  - "Deep sleep auf 5 Minuten" or "Display sleep 60 seconds"
  - "Audioqualitaet hoch" (16000 Hz) or "Sample rate 8000"
  - "Schuetteln aus" or "Disable shake"
  - "Gespraechsgedaechtnis aus" or "Turn off conversation memory"
- Settings take effect immediately where possible (volume, recording, parental lock).
- When parental lock is enabled, responses are blocked between 20:00 and 06:00, but settings changes (like disabling the lock) still work.
- Say "Start onboarding mode", "WLAN Einrichtung starten", "Open settings page", or similar to enter AP/setup mode by voice.

### Timers and reminders

- Set timers by speaking naturally:
  - "Erinnere mich in 90 Minuten an die Waschmaschine"
  - "Set pomodoro timer" (25 minutes)
  - "Timer auf 5 Minuten"
  - "Alert for tomorrow 5:30am"
- List active timers: "Liste mir die Timer" or "List pending timers"
- Clear all timers: "Loesche alle Timer" or "Cancel all timers"
- When you set a timer, a white card confirms it: a stopwatch with a check mark, the timer's name and when it rings (for example "in 10 Min").
- Timer list: one timer at a time on a white card with a stopwatch, its name and the time left (counting down live). Dots show which timer of how many. Works like the printer list: the ▼ button (Button C) scrolls, the trash button (Button B) deletes.
  Small tabs at the screen edge sit next to their buttons:
  - ✓ (next to Button A): exit timer list
  - ▼ (next to Button C): show next timer (only with two or more timers)
  - Trash can (next to Button B): delete the currently shown timer
- When a timer fires, the device plays an alert sound and shows a swinging bell with the reminder text, even if the display is asleep or in light sleep. The screen blinks white and black until you press Button A (✓ tab).
- Hold the stick upright and the timer card, the timer list and the alert turn to portrait with it.
- Maximum 5 concurrent timers. Active timers prevent deep sleep so they can fire on time.

### Answer sources and media
- Math and calculation questions are answered with Wolfram Alpha.
- Requests to create images use OpenAI image generation (GPT Image 2.5 Flare, falling back to GPT Image 1.5).
- If a related Wikipedia article is found, the device also shows an image.
- Time questions ("what time is it?") return the current time including seconds.

### Web setup (AP mode)
- If no WiFi is connected or no AI provider key is configured, the device creates an AP named "zitro".
- Connect to it — a captive portal should auto-open. If not, browse to `zitro.local` or `192.168.4.1`.
- Setup page actions:
  - **WiFi**: Add a network (SSID + password), reorder networks with Up/Down buttons, delete a network.
  - **AI**: Choose the provider (Zitro, OpenAI, Claude, Infomaniak, or OpenAI-compatible), paste keys, pick models. See "Choosing your AI".
  - **Settings**: Conversation memory, language (DE/EN), recording duration (1-30 seconds), audio quality (8 kHz standard or 16 kHz high), speaker volume, sleep timeouts, feature toggles and gesture controls. Out-of-range values are corrected to the nearest allowed value.
  - The device restarts after adding a network. AI and device settings are saved immediately and used from the next question on. After saving the AI tab, the page offers to restart Zitro; pressing Button A on the setup screen also leaves setup mode.

### Power and sleep
- After inactivity: display sleep ~30s, light sleep ~90s, deep sleep ~180s. The three times can be changed on the Settings tab and are used right away.
- Before sleeping, Zitro switches off WiFi, the screen, the motion sensor, the microphone and the speaker to save battery.
- Press Button A to wake. Deep sleep reboots and restores the last response if possible.
- With a reminder timer running, Zitro stays in light sleep (not deep sleep) so the alarm can ring on time; this uses more battery.

## User interface hierarchy

- Boot and connection
  - Power on or wake
  - WiFi connect attempts
  - AP setup mode (screen with SSID + IP, web setup page)
- Home / idle
  - "[lemon] Press [M5] button to ask a question" (M5 drawn as a key), tilted-phone shake icon + "Shake for random fact."
  - Battery indicator appears when low
  - Rotate icon appears when an image is available
- Question flow
  - Recording (large mic icon + progress)
    - Tap: "Restarting..." and record again
    - Double tap: cancel, back to the last answer
  - Transcribing (icon)
  - Thinking (large robot icon) / Calculating / Creating image / Finding image
  - Answer (text view)
  - Optional image view
    - Scroll controls
    - Zoom mode toggle
    - Printer discovery
    - Printer selection
    - Page size and zoom
    - Print result
- Timer / Reminder
  - Timer confirmation screen (white card: stopwatch with check, name, "in 10 Min")
  - Timer list view (white card, stopwatch; tabs: ✓ Button A back, ▼ Button C next, trash Button B delete)
  - Timer alert (swinging bell + text, blinking white/black, sound, Button A to dismiss)
  - All three also in portrait when the stick is held upright
- Sleep states
  - Display sleep
  - Light sleep
  - Deep sleep (restore last state on wake)

## Easy user guide

### Quick start
1. Power on the device and wait for WiFi to connect.
2. On the home screen, press Button A once and speak your question.
3. Wait for the answer on screen. The device records for about 5 seconds.

### Hear answers out loud
- Double click Button A to ask a question and hear the spoken response.

### Take a photo (with optional camera)
- Triple click Button A to capture and analyze a photo.
- Device will identify objects, plants, and share interesting facts.
- If no camera is attached, shows: "Kamera anschliessen, um Kamerafunktion zu nutzen".

### Surprise fact
- Shake the device to get a random fact (text only).

### Reading answers
- Empty lines between paragraphs take only half a line, so more text fits on screen.
- Long answers and facts are set with slightly tighter line spacing so they stay on screen.

### View images
- If an image is available, a rotate icon appears on the text screen.
- Toggle to the image by:
  - Rotating the device to the opposite orientation and back, or
  - Pressing Button C.
- Scroll a tall image:
  - Button B scrolls up.
  - Button C scrolls down.
- Zoom:
  - Roll the device to toggle contain vs cover zoom.

### Animated GIFs
- Animated GIFs play on the screen at their own speed. They keep playing while you scroll, zoom or turn the stick, and stop when another screen takes over. They pause while the display sleeps and go on when you wake it.
- A GIF stays a still picture (its first frame) when it has only one frame, is larger than about 350 x 350 pixels, or is over 300 KB, because Zitro only downloads the first 300 KB.
- Printing uses the first frame. See-through parts of a GIF print white and show black on the screen.

### Print an image
1. Switch to image view (the picture must be on screen, not the text).
2. Hold Button B for about 1 second to search for printers. Let go after 1 second: holding it 3 seconds resets WiFi.
3. The printer list shows each printer by its own name (for example "Canon TS5100 series") with a printer icon; the selected one is highlighted in white.
   Small tabs at the screen edge sit right next to the button they belong to:
   - ✓ (white tab, next to Button A): use the selected printer
   - ▼ (next to Button C): next printer (only shown with two or more printers)
   - ✕ (next to Button B): cancel
   Hold the stick with the front button on the left and Button C is at the top left, Button B at the bottom.
   Turn it around and the tabs move with the buttons: Button C at the bottom right, Button B at the top.
   Hold the stick upright and the list turns to portrait: one printer at a time as a big card, with dots showing which one of how many. The tabs still sit next to the buttons.
4. Pick the page size (the tabs work the same: ✓ print, ▼ next size, ✕ back to the printers). A small page picture in each row shows where your picture goes on the A4 sheet. Zitro remembers your last choice.
   - **Fit to page**: the whole picture, as large as it fits.
   - **Fill page**: fills the sheet; the edges of the picture get cut off.
   - **Half + text**: the whole picture in the top half, the answer in small text below it.
   - **Half page**: the whole picture in the top half, the rest stays empty.
   - **Zoom**: press ✓, then ▼ to step from 10% to 200% of the "Fit to page" size, and ✓ to print. Above 100% the edges get cut off.
5. A printer icon with a check (Sent!) or a cross (Print failed) shows the result, then the picture comes back.

Every page gets a small footnote at the bottom: where the picture comes from, and "Printed with" 🍋 https://zitro.spot. It follows the device language.

Good to know:
- Pages are A4, with margins of about 1 cm (the printer can't print to the very edge this way).
- Wikipedia pictures are small, so large prints look a bit blocky. "Half page" or a lower zoom looks sharper.
- Zitro asks each printer what it can take. Black-and-white printers get a grey page. If a printer refuses the page, Zitro tries another resolution, then grey, on its own. It may take a few seconds longer.
- A printer that takes none of these pages gets the plain picture file instead: no size choice applies and no footnote is added.

### WiFi setup and device configuration

1. Enter setup mode by either:
   - Waiting for the device to automatically enter AP mode (if WiFi fails or no API key is configured), or
   - Triple-clicking Button C from any screen to manually open settings.

2. Connect your phone or computer to "zitro" (or scan the QR code on the screen).
3. The setup page opens automatically (captive portal). If not, open `zitro.local` in your browser (or `192.168.4.1`).
4. **First-time setup**: A welcome screen leads you to the **AI** tab to choose a provider and paste its key, then to the **WiFi** tab.
5. Configure device settings:
   - Choose an AI provider and enter its key (required). See "Choosing your AI".
   - Turn conversation memory on or off (default: on)
   - Set language to German (DE) or English (EN)
   - Adjust recording duration (1-30 seconds, default 5)
   - Choose audio quality: Standard (8 kHz, faster uploads) or High (16 kHz, best quality)
   - Configure sleep timeouts (display, light, deep sleep)
   - Enable/disable features: image search, Wolfram Alpha, local quotes, parental lock
   - Enable/disable gestures: shake for random fact, double-click for audio, roll for zoom, rotation for toggle
   - Set speaker volume (0-255, default 210)
6. Add WiFi networks, reorder, or delete old ones.
7. Save settings. They apply from the next question on; adding a WiFi network restarts the device.

### Language and character support

The device natively supports text in 30+ languages with proper character rendering:
- **European languages**: German (ä,ö,ü,ß), French (é,è,à,ç), Spanish (ñ,á,ó), Polish (ą,ć,ł), Czech, and more
- **Greek alphabet**: Full support for Greek characters (Α,Β,Γ,Δ...)
- **Cyrillic script**: Russian, Ukrainian, Bulgarian (А,Б,В,Г...)
- **Hebrew**: Full Hebrew character set (א,ב,ג,ד...)
- **Character range**: 0x20-0x5FF (1,504 glyphs)

Characters display correctly without conversion - umlauts and special characters render as they should.

### Reset WiFi credentials
- Hold Button B for about 3 seconds in normal mode.
- Hold Button B for about 5 seconds in AP mode.

### Parental lock
- Say "Kindersicherung ein" to enable.
- Say "Kindersicherung aus" to disable.
- When enabled, the device goes to sleep between 20:00 and 06:00.

### Sleep and wake

- The display sleeps after about 30 seconds of inactivity.
- Light sleep starts after about 90 seconds, deep sleep after about 3 minutes.
- Press Button A to wake and show the previous result.
- Wait at least 2 seconds, then press Button A again to start a new recording. The press that wakes Zitro never starts a recording by itself.
- Deep sleep restarts and restores the last answer if it can.

## Screenshots of the device screens

With the stick connected over USB, run:

```bash
python3 scripts/capture_screens.py /dev/cu.usbserial-XXXX tour
```

Zitro draws each main screen once and sends it over USB. The PNGs land in `screenshots/states/`. Use `shot` instead of `tour` to capture only what is on screen right now.
