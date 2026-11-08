# Guided tour: Zitro shows what it can do

Date: 2026-11-08. Status: decided with Matthias through the mock.

Mock: `docs/superpowers/mocks/2026-11-08-zitro-tour-mock.html` (published privately at https://claude.ai/artifact/23QFJV23ysbLxcbqn1GNzx). Look and order come from it. Where this spec differs, this spec wins.

## Why

Zitro has so many features that nobody finds them by accident. A short tour lets a child ask one real question, shows three kinds of special answer, then shows what else is possible, including printing on paper.

## Goals

- After setup, the tour starts once on its own, for new sticks and for sticks that get it through an update.
- One real recording and one real AI answer. Everything after that uses fixed content and calls no AI.
- Nothing ends the tour by accident.
- It can be started again by voice.
- German and English, following the device language.

## Non-goals and anti-goals

- No quiz. "Interactive" means turning the stick. Grid tiles open nothing.
- No world wars anywhere in the tour. Examples stay on space.
- It fails, even if it runs, when a stick lying on a desk stays awake and drains its battery, when a button press on a dark screen does something other than wake it, or when fast presses on ▶ open the settings.

## Flow

| Step | Screen | C | M5 (A) | B |
| --- | --- | --- | --- | --- |
| Ask | Lemon, "Hallo, ich bin Zitro!", "Drück [M5] und sprich:", the suggested question | none | record | menu |
| Recording | Existing boxes, plus "Sprich jetzt!" | none | as today: restart, double tap cancels | none |
| Heard | "Du hast gesagt:" and the transcript, 1.8 s, then the existing "Thinking..." | none | none | none |
| Retry (not understood) | "Das habe ich nicht verstanden." | ▶ to Photo | record | menu |
| Answer | The real answer, or the real view if the AI opened one | ▶ | menu | menu |
| Photo | Quote "Wie sieht der Mond aus?", then the Moon with "Stell mich hochkant!" | ▶ | menu | menu |
| Sky | Quote "Wo ist Orion?", then the real star map, `sky:Orion` | ▶ | menu | menu |
| Music | Quote "Spiel Twinkle Twinkle Little Star", then the real band, `music:twinkle-twinkle` | ▶ | menu | menu |
| Grid | 4 pages of 2x2 tiles | next page, then Done | menu | menu |
| Done | "Jetzt du!" | none | ends the tour and starts a normal question | menu |

- A quote screen lasts 1.9 s. C skips it. Going back never shows a quote.
- Tilting the stick slides the grid pages in both directions, with the timeline's tilt gesture.
- Standing the stick upright and back toggles the Moon picture and its text, the same gesture as everywhere else.

## The menu

Opened by M5 or B. Rows: **Weiter** (preselected), **Ein Schritt zurück**, **Tour beenden** (English: Continue, One step back, End tour). On Ask there is no "back" row. It works like the printer list: A confirms, C goes to the next row and wraps, B closes it. Pressing M5 twice therefore continues harmlessly.

One step back: Grid page n to n-1, Grid page 1 to Music, Photo to Answer (to Retry if no answer came), Answer and Retry to Ask, Done to the last Grid page, Sky to Photo, Music to Sky. Music goes silent while the menu is open and carries on afterwards.

## When it starts and ends

- Starts at the end of `setup()` when the flag `tourDone` is false, the boot isn't a wake from deep sleep, AI and voice keys are configured, and AP mode isn't running.
- "Tour beenden" and finishing both set `tourDone` (namespace `settings`). Afterwards the home screen shows, and all tour content is cleared so nothing of it comes back after a wake.
- The first question can end without an answer. Not heard or mic error: Retry. Cancelled: back to Ask. Blocked by the parental lock, missing key or a request for setup mode: the tour stops and `tourDone` stays false.
- Started again by voice: the AI sets `is_tour` for "Was kannst du?", "Zeig mir die Tour", "What can you do?", "Show me the tour". "Start onboarding mode" keeps opening WiFi setup. A voice start ignores `tourDone`.

## Sleep

Each press keeps the stick awake for 5 minutes (same as `MODEL_MOTION_CAP_MS`). After that the normal sleep timers apply. A press on a dark screen or during the wake cooldown only wakes the screen and redraws the step. After a deep sleep the tour is gone and comes back at the next power-on, because `tourDone` is still false.

## Button tabs

Tabs stay visible for the whole tour, because ▶ is the only way forward. C shows ▶, B shows a new three-dot symbol, A shows the microphone only on Ask, Retry and Done. In the menu: ✓, ▼, ✕.

## Grid tiles

Page order, title and sentence, German and English. Sentences use the ASCII font (`DejaVu9`), so they avoid umlauts and superscripts: this is why some differ from the mock. Titles use the 12 pt font, which has umlauts.

| Page | DE title | DE sentence | EN title | EN sentence |
| --- | --- | --- | --- | --- |
| 1 | Drucken | Bild zeigen, B 1 Sek. halten | Print | Show picture, hold B 1 s |
| 1 | Zeitstrahl | Zeitstrahl der Raumfahrt | Timeline | Timeline of spaceflight |
| 1 | Moleküle | Wie ist Koffein aufgebaut? | Molecule | How is caffeine built? |
| 1 | Körper | Was ist im Herz? | Body | What's inside the heart? |
| 2 | Globus | Wo liegt das Matterhorn? | Globe | Where is the Matterhorn? |
| 2 | ISS live | Wo ist die ISS gerade? | ISS live | Where is the ISS? |
| 2 | Physik | Wie funktioniert ein Hebel? | Physics | How does a lever work? |
| 2 | Kurven | Zeichne sin(x) | Curves | Draw sin(x) |
| 3 | Rechnen | Was ist 12 mal 34? | Math | What is 12 times 34? |
| 3 | Timer | Timer auf 5 Minuten | Timer | Timer for 5 minutes |
| 3 | Rezepte | Wie mache ich Pfannkuchen? | Recipes | How do I make pancakes? |
| 3 | Weltuhr | Wie viel Uhr ist es in Tokio? | Clock | What time is it in Tokyo? |
| 4 | Vorlesen | M5 zweimal schnell | Speak | Press M5 twice |
| 4 | Schütteln | Bringt etwas Neues | Shake | For a surprise |
| 4 | KI-Bilder | Male einen Drachen mit Hut | AI art | Paint a dragon with a hat |
| 4 | Kamera | M5 dreimal, mit Kamera-Modul | Camera | M5 3x, with camera module |

Open for a check on the real stick: at 9 px the sentences are about 1 mm high. If they're too small for early readers, the fallback agreed in the mock is picture and one word only.

## The Moon picture

`File:Moon nearside LRO.jpg` from Wikimedia Commons, public domain, by NASA/GSFC/Arizona State University (checked 2026-11-08 through the Commons API). Built into the firmware as a baseline JPEG on black, 240x135, disc 96 px. Bottom line: "Mond - NASA" / "Moon - NASA". The tour draws it itself: `displayImage()` has no way to add tabs, and it would replace the picture of the child's own answer.

## Text strings

| Key | DE | EN |
| --- | --- | --- |
| hello | Hallo, ich bin Zitro! | Hi, I'm Zitro! |
| press before / after | Drück / und sprich: | Press / and say: |
| suggestion | «Wie weit ist der Mond weg?» | "How far away is the Moon?" |
| speak | Sprich jetzt! | Speak now! |
| heard | Du hast gesagt: | You said: |
| retry | Das habe ich nicht verstanden. | I didn't catch that. |
| retry press after | nochmal | again |
| you ask | Du fragst: | You ask: |
| photo quote | Wie sieht der Mond aus? | What does the Moon look like? |
| sky quote | Wo ist Orion? | Where is Orion? |
| music quote | Spiel Twinkle Twinkle Little Star | Play Twinkle Twinkle Little Star |
| photo hint | Stell mich hochkant! | Stand me upright! |
| photo text | Der Mond ist grau und voller Krater. Sie stammen von Einschlägen. | The Moon is grey and full of craters. They come from impacts. |
| done | Jetzt du! | Your turn! |
| done sub | Frag mich, was du willst. | Ask me anything. |

## Changes to documented behaviour

Approved through the mock:

- In the tour M5 opens the menu instead of asking something new, except on Ask, Retry and Done.
- In the tour C is always ▶ and B opens the menu, whatever they do in that view elsewhere.
- Tabs don't fade after 3 s during the tour.

Proposed in this spec, to be confirmed by Matthias:

- Three presses of C don't open the settings during the tour, because C is ▶.
- Holding B doesn't reset WiFi during the tour.
