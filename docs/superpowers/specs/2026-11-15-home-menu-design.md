# The home menu: functions, a line, and 16 example questions: design

Short design (small change). 2026-11-15, from the conversation with the user in session ai-stick-ef.

## Intent

The user: "Beim Hauptscreen der Button B soll das Menü zeigen, mit Settings, aber auch mit 10+ Fragen, die die Multimedia-Sachen auslösen."
One press of B on the home screen opens a menu, on both sticks. It holds the stick's functions and, below a horizontal line, example questions. Each question opens one of Zitro's multimedia views without speaking.

Asked whether a stray press of A may cost network and AI calls, the user asked: "Kann man die Liste statisch haben, oder einmal herunterladen und behalten?" So the questions and their answers are static, and choosing one calls no AI.

**Changed later the same day by the user.** At first the questions sat in a sub-list behind a row "Ausprobieren" in the button module's menu. The user then asked for:
- "one prominent list item should be the 'show alerts list', and other such functional elements";
- the functions "direkt ins Menü nach B";
- "Statt 'Ausprobieren' als Unterliste, einfach eine horizontale Linie und dann alle 16 Sachen zeigen. Und ein Menüpunkt 'Zurück' / 'Menü schliessen' o.ä.";
- as functions: Tour starten, Überraschung, Wörter üben.

So the home menu is one list, and this work owns it: the button module (ai-stick-0a) opens it with one press of B on the home screen, instead of its shared menu.

## The menu

| Row | DE | EN | Does |
|---|---|---|---|
| 1 | Timer anzeigen | Show timers | the timer list, or "Keine Timer aktiv." (as the spoken "Welche Timer laufen?") |
| 2 | Tour starten | Start the tour | the guided tour (as the spoken "Was kannst du?") |
| 3 | Überraschung | Surprise | a surprise, as shaking gives one (also when shaking is off in the settings) |
| 4 | Wörter üben | Practise words | the vocabulary trainer: the list practised last, else the choice (as "Ich will Wörter üben") |
| 5 | Einstellungen | Settings | the setup page, as the triple press does today (`openSettings()`) |
| 6 | Menü schliessen | Close menu | back to the home screen |
| — | (horizontal line) | | |
| 7–22 | the 16 questions below | | |

- **Preselected:** row 1. A stray A shows the timers, which is harmless.
- **"WLAN zurücksetzen"** isn't in the user's list. The setup page sets WiFi, and the module keeps the row in the menus of answers.
- **Night lock (8 pm to 6 am, when on):** the timers, the settings and closing still work. The tour, the surprise, the trainer and the 16 questions show "Ich schlafe z z z", as a spoken question does. My call: the lock is for learning and playing, and a spoken "Was kannst du?" is blocked too.

## Goals

- 16 questions, one per view, space first. The user approved the list and changed #5 to Marie Curie.
- Choosing a question asks the AI nothing, needs no AI key, and costs nothing.
  - The view still fetches its data from its free source as usual (Wikipedia, Wikidata, PubChem and the others), so WiFi is needed.
  - Live views (ISS, sky, clock) compute fresh each time.
- Labels and answer texts follow the language setting.
- The list works the same on the StickS3 and on the Plus2.

## Non-goals

- **Which press opens the menu.** That belongs to the button module (ai-stick-0a, `feature/button-ui`): one press of B on the home screen calls `homeMenuOpen()`, on both sticks. Until the module lands, the menu opens over serial (`home`).
- **The Plus2 press scheme.** The user chose (2026-11-15) that the Plus2 presses like the StickS3, with C read nowhere. That is the module's work.
- **Downloading or editing the list.** Approach 2 below; it can come later.

## Anti-goals

- A press that costs money: no Wolfram (own paid API with a quota), no AI-painted pictures, no spoken answer (text-to-speech is a paid API call; the shake surprise doesn't speak either).
- Facts nobody checked. The answer text is a short line ("Hier ist Orion."); the facts come from the view's sources (see the educational media direction: displayed facts come from real sources).
- War topics (the user's rule for the tour).

## Approaches

1. **Static in the firmware (chosen).** Like the shake surprise (`surprise_data.ino`), the firmware holds each question's label, answer text (DE/EN) and what the view looks up. The tour already opens views this way, from a fixed reference (`guideOpenView`: `parseModelRef` + `fetchModel`).
2. **Download once and keep.** The stick fetches the list from zitro.spot once and stores it. The questions could change without a firmware update, but it needs a server endpoint and more code. Not now.
3. **Ask the AI live.** Every choice is a normal question. It costs an AI call per press, also for a stray A. Rejected by the user's question above.

## The questions

Label width: the menu font (Roboto 10 pt), from x 50 to the end of the row at x 224, at most 174 px. Measured with the font's glyph advances.

| # | Label DE | Label EN | View | Looks up |
|---|---|---|---|---|
| 1 | Wo ist Orion? (118 px) | Where is Orion? (140) | Sky | `sky:Orion` |
| 2 | Wo ist die ISS? (131) | Where is the ISS? (155) | ISS | `iss:now` |
| 3 | Mondlandungen (143) | Moon landings (130) | Timeline | `timeline:moon_missions` |
| 4 | Mondphasen (115) | Moon phases (119) | Physics | `physics:moon` |
| 5 | Foto: Marie Curie (153) | Photo: Marie Curie (166) | Photo | Wikipedia picture "Marie Curie" |
| 6 | T-Rex (51) | T. rex (50) | Dino | `dino:Tyrannosaurus` |
| 7 | Das Herz (81) | The heart (86) | Anatomy | `anatomy:heart` |
| 8 | DNA in 3D (90) | DNA in 3D (90) | Protein | `protein:dna` |
| 9 | Salzkristall (98) | Salt crystal (99) | Crystal | `crystal:salt` |
| 10 | Wassermolekül (138) | Water molecule (141) | Molecule | water, in the molecule feature's form |
| 11 | Ikosaeder (87) | Icosahedron (109) | Solid | `solid:icosahedron` |
| 12 | Kurve von x² (111) | Graph of x² (99) | Graph | `graph:` x², in the graph feature's form |
| 13 | Matterhorn (100) | The Matterhorn (139) | Globe | `globe:Matterhorn` |
| 14 | Uhrzeit in Tokio (139) | Time in Tokyo (125) | World clock | `clock:Tokyo` |
| 15 | Pfannkuchen (117) | Pancakes (87) | Recipe | `recipe:pancakes` |
| 16 | Twinkle Twinkle (143) | Twinkle Twinkle (143) | Music | `music:twinkle-twinkle` (the tour's song) |

The user asked for "Foto von Marie Curie" (185 px, too wide for the row), so the label is "Foto: Marie Curie".

## The menu screen

- **Look:** as the menus of the trainer and the tour (`vocabDrawMenu`, `guideDrawMenu`). Landscape, 3 rows visible, the selected row a white rounded rectangle 204 × 30 at x 20, label at x 50, a scroll bar at x 230. A grey horizontal line above the first question, wherever it is visible.
- **Presses, both sticks:** B once = next row (wraps), B twice or held = back to the home screen, A = choose.
  - **Changed 2026-11-16 after the user's hand test** ("in the menu double button b shouldn't have it's own function"; the X for C was still shown; quick presses closed the menu and later opened the setup page): every click of B is the next row at once (`menuPressRead()`, raw `M5.BtnB.wasClicked()`), holding B closes, C isn't read and has no tab, and B's clicks are forgotten as they come. The trainer's and the shop's lists press the same way ("Bei Voki üben ist die Menüliste immer noch mit Btn C für runter"). Read as the trainer reads them today (`buttonBClicked()`, `buttonCClicked()`, `M5.BtnB.wasHold()`); the module later switches all of them to its reader (agreed with ai-stick-0a).
- **Entry:** `homeMenuOpen()`, `homeMenuActive()`. Serial for testing, as `ask` is: `home` opens the menu, `home <n>` chooses row n, `homekey a|b|c` presses.

## Choosing a question

- **Night lock (8 pm to 6 am):** blocked as a spoken question is ("Ich schlafe z z z"). My call: the list must not get around the parental lock.
- **WiFi:** like a spoken question: if it's still searching, the list waits the way "Transcribing..." does.
- **The answer:** the same code that shows a spoken question's answer, fed with the fixed answer instead of the AI's. The code after `askGPT` (fixed reference, `fetchModel`, the dino and music lines, the clock and ISS answers that are always Zitro's own, the `imageQuery` picture) moves into one function that both the voice path and the list call. So a list answer behaves exactly like a spoken one: view, text, tabs, print, memory of the conversation, so a spoken follow-up ("Wie gross war er?") knows the topic.
- **No speech** (see anti-goals).

## Left to the builder

- The exact molecule and graph references (by the features' own parsers; the host test checks them).
- Icons next to the labels: none needed. The builder may take the view's mark where one exists.

## Tests

- **Host test** over the table: every reference parses (`parseModelRef`), its kind is in `MODEL_KINDS`, its id is in that feature's allowed list, and every label fits 174 px in the menu font.
- **On the StickS3:** each of the 16 questions opens its view, in German and in English. There is no Plus2 to test on.

## Docs

REQUIREMENTS (new section "Home menu"), USERGUIDE (the home menu), RELEASE_NOTES.

## Decided by the user

- The 16 questions (#5 changed to Marie Curie), static, no AI.
- Order of work: the module and this menu are built at the same time; this menu is tested over serial first.
- The Plus2 presses like the StickS3, C read nowhere (the module's work).
- One menu, functions first ("Timer anzeigen" on top), a line, the 16 questions, and a row to close.
