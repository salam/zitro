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
  - Short click (image view): scroll. The upper of the two side buttons scrolls up, the lower one down, however you hold the stick.
  - Hold about 1 second (image view): open printer discovery and selection.
  - Hold about 3 seconds (normal mode): reset WiFi credentials and restart.
  - Hold about 5 seconds (AP mode): reset WiFi credentials and restart.
  - Print menu: one step back; in the printer list, cancel.
  - Timer list: delete the shown timer.
- Button C / PWR (bottom)
  - Triple click: open settings/setup page (enters AP mode). Only where Button C has no other job: not in the printer list, timer list or a timer alert, and not while the answer has a picture or 3D model, so scrolling can't open the settings by accident. There, say "Open settings page" instead.
  - Short click (image view): scroll (see Button B).
  - Short click (text view with image available): toggle text vs image view.
  - Print menu: next printer, page size or zoom value.
  - Timer list: show the next timer.

### Gestures
- Shake: a surprise, no voice playback. Half the time a fact, otherwise a scene, a shape, a star map or the time in a big city, all built in, so it works without WiFi. Switch it off in the settings ("Schuetteln aus").
- Rotate: rotate to the opposite orientation and back to toggle text vs image.
- Roll: toggles image zoom mode (contain vs cover) while image is shown.
- To save battery, the gyroscope (needed for roll and for turning 3D models) only runs while an image or 3D model is on screen. Shake and turning the stick still work everywhere.

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
  - "Stelle meinen Wohnort auf Berlin" or "Set my home city to Tokyo" (also sets the time zone)
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

### 3D molecules

Ask how a substance is built, or ask for it in 3D:
- "Wie ist Koffein aufgebaut?" or "How is caffeine built?"
- "Zeig mir Aspirin in 3D" or "Show me aspirin in 3D"
- "Visualisiere Koffein" or "Visualize caffeine"

1. The model opens right away.
2. Turn the stick to look around the molecule. It stays where you turned it. Put it down and after 3 seconds the model turns slowly by itself.
3. Press Button B to zoom (3 steps). Press Button A to ask something new.
4. Press Button C to read the answer. A small cube at the bottom right means the model is still there: press Button C again, or rotate the stick to the opposite orientation and back.

Good to know:
- The bottom line names the molecule and its source, and shows which color is which atom: C, H, O, N and so on.
- All 3D data comes from PubChem. The AI only picks the name to look up, so a model is never made up.
- PubChem has no 3D model for some things, such as table salt, DNA and proteins. Crystals and proteins have their own views, below; for anything else you get the usual picture.
- General questions like "What is water?" still show a picture. Only pictures can be printed, so a 3D answer can't be printed yet.
- Shake, roll and rotate don't do anything while the model is on screen, so turning it can't trigger them.
- While you keep turning, the screen stays on, for up to 5 minutes after your last button press. Put the stick down and it goes to sleep as usual.

### More than pictures

Zitro can show much more than pictures. You don't choose a mode: just ask. Everything shown comes from a real source or exact math, and the bottom line names it. If there's nothing, you get the usual picture. Button A always asks something new.

When a result opens, small tabs at the buttons show what each one does: a microphone at Button A for a new question, a magnifier for zoom, lines of text for the written answer, arrows for next and back. On a timeline the arrows point left and right, the way the years run. After 3 seconds they disappear so you can enjoy the whole picture. Press Button B or C and they show up again for a moment.

Proteins, anatomy, recipes, timelines and the detailed globe load their data from zitro.spot, so they need WiFi.

#### Music on an endless band

Ask for a song by name, for example "Spiel Alle meine Entchen", "Wie geht Bruder Jakob?" or "Play Silent Night". The notes move across the screen and the one on the line plays.

Zitro knows 26 songs by heart, listed below, and they work without WiFi. When it is connected, it also finds 18 more on its own server, zitro.spot, such as Oh! Susanna and Der Mond ist aufgegangen. Every server song comes from an old printed songbook and is checked note by note against a second score, and `https://zitro.spot/data/music/SOURCES.txt` shows where each one comes from. The footer names the source, for example "Oh! Susanna - Holt 1848". Zitro has no recordings and finds a song by its title. Capital letters, umlaut spelling and punctuation don't matter, and some songs have a second name that works too, but a near miss such as a misspelled word isn't guessed. If it doesn't know a song, it says "I don't know that song. Try ...!" and suggests one it knows. If it can't reach its song list, it says "I can't reach my song list right now. Try ...!". Either way you still get a picture. Ask for "Ode to Joy" to hear the famous tune from Beethoven's Ninth. If you just ask for "Beethoven's symphony", Zitro may play that tune, or say it doesn't know it and suggest another song.

German: Alle meine Entchen, Hänschen klein, Fuchs du hast die Gans gestohlen, Kommt ein Vogel geflogen, Ein Männlein steht im Walde, Alle Vögel sind schon da, Der Kuckuck und der Esel, Kuckuck Kuckuck ruft's aus dem Wald, Backe backe Kuchen, Schlaf Kindlein schlaf, Guten Abend gut' Nacht, Häschen in der Grube, Summ summ summ, O Tannenbaum, Stille Nacht, Schneeflöckchen Weißröckchen, Wer hat die schönsten Schäfchen, Hänsel und Gretel, Bruder Jakob, Zum Geburtstag viel Glück.

English and others: Twinkle Twinkle Little Star, Row Row Row Your Boat, London Bridge, Jingle Bells, Ode to Joy, Here We Go Round the Mulberry Bush.

- **Button A:** new question.
- **Button B:** pause. Press again to carry on.
- **Double-click Button B:** colourful note heads on or off. Every note gets its own colour, as on Boomwhackers: C red, D orange, E yellow, F green, G teal, A indigo, B violet (H in German). A sharp or flat keeps the colour of its letter, so F♯ is green. The colours switch about a second after your double click, and the song only stops for a moment. Zitro remembers your choice for the next song. Pausing and carrying on within a second counts as a double click too, so if the colours appear by surprise, double-click again.
- **Button C:** change the volume: silent, low, regular, loud, and round again. The speaker symbol at the button shows the level for a moment: no sound waves means silent. Each new song starts loud.
- **Hold Button C for a second:** show the text answer and stop the music. Press Button C once to hear the song from the start.

The music plays with one click on Button A, and loud means your normal volume from the settings. It keeps going round until you stop it, and pauses by itself after 5 minutes.

Jingle Bells plays the chorus only. Stille Nacht is the version sung today.

#### Function graphs

Ask Zitro to show a curve, for example "Visualisiere sin(x)", "Zeichne x^2 minus 3" or "Wie sieht die Kurve von 1 durch x aus?". Zitro draws the graph itself and calculates every point exactly.

- **Tilt the stick** left or right and hold it to slide along the curve. Hold it level again to stop. The top line shows x and y at the white dot.
- **Button B** zooms: closer, then far away, then back to where you started.
- **Button C** shows the text answer. Press it again to return to the graph.
- **Button A** asks a new question.

Where a curve has no value, the line stops. That's correct: `1/x` has no value at 0, so there's a gap.

Questions like "Was ist das Integral von sin(x)?" or "Loese x^2=4" still go to Wolfram Alpha.

#### Recipes step by step

Ask how to make something: "Wie mache ich Pfannkuchen?" or "How do I make pizza?". If Zitro has that recipe, it opens right away. If not, you get the usual picture.

First you see what you need, then one step at a time.

- **Button C**: next. After the last step you're back at the text answer.
- **Button B**: back.
- **Button A**: new question, as always.

"Schritt 3/8" means step 3 of 8. A small number at the bottom right, such as 2/3, means this step goes on: press Button C.

An orange **"Erwachsene fragen"** marker means the step has something hot or sharp in it, or the oven. Ask a grown-up to help. Without the marker you should still take care: Zitro only looks for certain words.

There are 59 dishes, such as pancakes, pizza, soups, cakes and cookies. 37 of them are in German and 40 in English. If a recipe only exists in the other language, Zitro shows it in that language and says so at the bottom, for example "Wikibooks (EN)". Recipes with alcohol are left out.

The recipes come from Wikibooks, written by volunteers. They aren't all written for children, so some steps are long and use cooks' words.

#### Globe with a pin

Ask where a place is: "Where is the Matterhorn?" or "Wo liegt Paris?". Zitro shows the Earth with a yellow pin on that place.

- **Turn the stick** to turn the globe.
- **Flick it quickly** and the globe keeps spinning, then slows down. Move the stick again to stop it.
- **Lost the pin?** A yellow arrow shows which way it is.
- **Button B** zooms in, three steps.
- **Button C** shows the written answer.
- **Button A** asks a new question.

Zitro only shows a globe for places Wikipedia has a position for, on Earth. For anything else, such as Mars, you get the picture. "What does the Matterhorn look like?" also gets a picture.

The dark side of the globe is where it's night right now.

#### World clock

Ask for the time: "Wie spät ist es in Tokio?", "What time is it in New York?", or just "Wie spät ist es?".

Zitro shows the Earth turned to that city, with a yellow pin. The time there is big at the top left, your own time below it, and how many hours apart they are. The dark side of the Earth is where it's night right now, and a little sun marks where the Sun stands straight overhead. Faint lines show the time zones, 15 degrees each.

- **Turn the stick** to turn the globe; **flick** it to spin.
- **Button B** zooms in.
- **Button C** shows the answer as text.

Zitro knows 320 cities: every capital and the biggest cities of the world. It works out the time itself, summer time included, so it needs to have been online once to set its clock. For a city it doesn't know, it says so.

Set your own city on the setup page under **Home city**, or say "Stelle meinen Wohnort auf Berlin". It also sets your Zitro's time zone.

#### Proteins, DNA and crystals

Ask how something is built, or ask for 3D:

- "Wie ist Hämoglobin aufgebaut?", "Zeig mir DNA in 3D"
- "Wie ist Kochsalz aufgebaut?", "Show me a diamond in 3D"

Zitro then shows a real model that you turn with your hand. It stays where you turned it; after 3 seconds lying still, it turns slowly by itself.

**Proteins and DNA** are a coloured ribbon that follows the molecule's chain. Each chain starts blue and ends red. Zitro knows about 50 famous ones, such as hemoglobin, insulin, lysozyme, GFP, antibodies, enzymes like pepsin and amylase, and DNA. Is yours not there? You get the usual picture. Models come from the Zitro server, so they need WiFi.

**Crystals** show the atoms in their real places, each element in its own colour. Zitro knows salt, diamond, graphite, ice, quartz, calcite, pyrite, fluorite, copper, gold, silver, iron and silicon. Ice shows only its oxygen atoms.

Questions like "What is an enzyme?" or "What is a diamond?" still get a photo.

The bottom line names what you see and where it comes from, such as "Haemoglobin - RCSB PDB 4HHB".

| Button | What it does |
| --- | --- |
| A | Ask something new |
| B | Zoom in, three steps |
| C | Show the written answer |

#### Night sky

Ask about a constellation or the stars: "Where is Orion?", "What does the Great Bear look like?", "Show me the night sky."

Zitro opens a star map. Turn the stick and look around. The stars move when you turn, and stay put when you stop.

The constellation you asked for is drawn in yellow. If you lose it, follow the yellow arrow.

- **Button A:** new question
- **Button B:** zoom in, three steps, then back
- **Button C:** back to the written answer

This is a map of all the stars, not a window. The stick has no compass and doesn't know where you are, so it can't point at the real stars above you. It shows the stars as they sit on the whole sky, all year round, without planets or the Moon.

#### Physics scenes and geometric solids

Ask how something works, and Zitro shows it moving.

- "Wie funktioniert ein Hebel?", "Warum schwimmt Holz?", "Warum gibt es Jahreszeiten?", "Warum knickt ein Stab im Wasser?", "Wie ist ein Kohlenstoffatom aufgebaut?"
- For shapes: "Zeig mir einen Torus", "Wie sieht ein Ikosaeder aus?" Just "Pyramide" or "Prisma" shows the square pyramid and the triangular prism.

Questions like "Was ist ein Magnet?" still bring a photo.

**Scenes:** Button B changes something: a heavier weight, a longer pendulum, another planet. Button C shows the written answer, and pressing it again brings the scene back. Button A asks something new.

**Shapes:** turn the stick to look around the shape. Button B zooms. Shapes with flat sides show how many faces, edges and corners they have. Button C shows the written answer.

#### Timelines with pictures

Ask for a timeline: "Zeig mir den Zeitstrahl der Weltkriege" or "Show me a timeline of space exploration".

Topics: world wars, space exploration, dinosaurs, Roman Empire, inventions, Swiss history, ancient Egypt, ancient Greece, Middle Ages, explorers, aviation, railways, computers, Moon missions, Mars missions, Olympic Games, medicine, scientists, composers, painters, Earth's history, Cold War, famous buildings, communication, cars.

- **Button C:** next event. After the last one you're back at the answer.
- **Button B:** previous event.
- **Tip the stick** to the right for the next event, to the left for the previous one.
- **Button A:** new question.

The big year runs while the band moves. A long empty stretch between two pictures means a long time. A small cut in the line at the top means even longer than it looks.

#### Organs, skeleton and cells

Ask for a body part in 3D, or ask what's inside: "Zeig mir das Herz in 3D", "Was ist im Herz?", "Wie sieht das Auge innen aus?", "Wie ist eine Pflanzenzelle aufgebaut?"

Zitro knows the heart, brain, lungs, stomach, liver, kidney, skeleton, skull, eye and hand bones. It also knows the animal cell, plant cell, red blood cell and nerve cell. For anything else, or when a model can't be loaded, you see a picture as usual.

Turn the stick and the model turns with it. Button B zooms in. Button A asks something new.

Button C shows the other view: from the outside to the inside, or the other way round. Press it again for the text answer.

The inside view is cut open, with the name of each part beside it in its colour. It always stays turned towards you. Zoom in and the words make room for the model.

Heart, brain, kidney, eye, liver, lungs and skull have a real inside, from anatomy research, made simple enough for this small screen. With liver and lungs, part of the tissue is taken away so you can see the tubes inside: the screen says "Gewebe entfernt". The stomach, skeleton and hand have no inside view, because the research models don't have one.

The cells are always shown open. They are drawn from what school books describe, and they carry the sign "Vereinfachtes Schema", because they are simple drawings and not measurements.

#### Calculations and formulas

Ask Zitro to calculate something: "Was ist die Ableitung von x^2 sin(x)?", "Löse x^2 + x - 1 = 0", "Wie gross ist das Volumen eines Torus mit Radius 5 cm?"

The result opens right away, with real fractions, powers and roots. The small grey lines say what Wolfram assumed, like "assuming inner radius b": they matter, so read them.

- **Button C**: next page. After the last page you see Zitro's text answer. Press again to go back to the start.
- **Button B**: one page back.
- **Hold Button B**: to print. Zitro fetches Wolfram's full picture of the result and opens the printer list. The print shows that picture, not the formula on the screen, and the picture stays on screen afterwards.
- **Button A**: new question.

The numbers come straight from Wolfram|Alpha, unchanged, and a long number carries on in the next line. The bottom line shows what Wolfram understood. If Wolfram's answer can't be shown as text, you get the Wolfram picture as before. Some answers have a plot too: it comes as a page of its own.

### Answer sources and media
- Calculations and formulas come from Wolfram Alpha, shown as sharp text. Plain function plots ("Draw x^2") get Zitro's own graph.
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
  - Power on or wake (the logo appears; two playful little pops sound on power-on and restart, at your speaker volume, and stay quiet when waking from deep sleep)
  - WiFi connect attempts
  - AP setup mode (screen with SSID + IP, web setup page)
- Home / idle
  - "[lemon] Press [M5] button to ask a question" (M5 drawn as a key), tilted-phone shake icon + "Shake for a surprise."
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

### Surprise
- Shake the device for a surprise: a random fact, or a scene, shape, star map or world time to look at. No voice.

### Reading answers
- Empty lines between paragraphs take only half a line, so more text fits on screen.
- Long answers and facts are set with slightly tighter line spacing so they stay on screen.

### View images
- If an image is available, a rotate icon appears on the text screen.
- Toggle to the image by:
  - Rotating the device to the opposite orientation and back, or
  - Pressing Button C.
- Scroll a tall image:
  - The upper side button scrolls up, the lower one scrolls down, however you hold the stick.
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
   - Triple-clicking Button C (not while a list, picture or 3D model is in use), or saying "Open settings page".

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
   - Enable/disable gestures: shake for a surprise, double-click for audio, roll for zoom, rotation for toggle
   - Home city: sets the time zone (summer time included) and "here" for the world clock (default Zurich)
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
- Deep sleep restarts and restores the last answer, with its picture or 3D model if it can. Pictures Zitro drew are kept on the stick for this; other pictures and models are fetched again. A Wolfram picture isn't kept, so that answer comes back without it.

## Screenshots of the device screens

With the stick connected over USB, run:

```bash
python3 scripts/capture_screens.py /dev/cu.usbserial-XXXX tour
```

Zitro draws each main screen once and sends it over USB. The PNGs land in `screenshots/states/`. Use `shot` instead of `tour` to capture only what is on screen right now.
