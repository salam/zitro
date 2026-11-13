# Vocabulary trainer (design)

Date: 2026-11-13. Status: approved in conversation ("Passt alles, ja. Keep developing."). Mock:
https://claude.ai/artifact/61u95osH33XJz32c8CKos5 (private to the owner).

## 1. Intent

German-speaking kids practise the English words their school book gives them, unit by unit, with Zitro.
A child says "Unit 3 Englisch üben", Zitro opens a trainer with exactly that unit's words, asks them one by
one, listens to the answer and says the right word aloud. A spelling mode trains writing, and a printed sheet
("Voci-Blatt") gives real pen-and-paper practice.

Zitro ships no word lists. Parents upload their own files to zitro.spot, tied to their stick by a code the
stick shows as a QR code. The test case is Young World 2 to 4 (grades 4 to 6), units 1 to 5, used as if a
parent had uploaded them.

This is part 1 (lists) and part 2 (trainer) of four. Part 3, a shop role-play with the vocabulary
("Verkäuferlis"), and part 4, quizzes on any topic with chained AI answers, come later and are out of scope.
The list storage and lookup here should be reusable by part 3.

## 2. Decisions by the user (2026-11-13)

- Vocabulary trainer and quizzes on the display are wanted. This reverses the rejection of an on-screen quiz
  mode from 2026-11-05.
- No lists from Zitro. Users upload their own files (CSV and other standard vocabulary formats); zitro.spot
  serves them. Public sources to pick from: only if a lawful one exists (none found, see 5.4).
- Lists belong to a stick through a code the stick shows as a QR code. No account (approach A).
- Uploaded files are parsed on the server (not in the browser).
- The trainer is its own mode that takes over the buttons, like the guided tour.
- The AI names only book and unit; the stick finds the list itself, like songs and recipes.
- Spelling mode: the child spells the whole word in one recording, then the letter boxes fill one after
  another (way A). The empty boxes, one per letter, are the gap text and show the word's length up front.
- The printed sheet is wanted ("Druck-Idee ist perfekt").
- My proposals the user accepted without change: list privacy per stick, book/unit/current book, English
  first with language-neutral storage, deletion after a year without use, round rules, lenient sound-alike
  letters, AI judging only for sentences with gaps.

## 3. Goals, non-goals, anti-goals

Goals
- A parent gets a unit onto the stick in under two minutes from a phone, without an account.
- A child can practise a unit alone: start by voice, answer by voice, hear the right pronunciation.
- Every right answer shown or spoken comes from the uploaded list, never from the AI.
- Spelling practice that marks exactly which letter is missing or wrong.
- A printable sheet with solutions.

Non-goals
- Shipping or hosting any publisher's word lists. Sharing lists between families.
- Pictures for words, the shop role-play, general quizzes (parts 3 and 4).
- Languages other than English as the trained language in this round. Storage and parsing stay
  language-neutral so French can follow.
- Spaced-repetition scheduling beyond "wrong words come first next time".

Anti-goals (failures even if it works)
- The child is marked wrong because Zitro misheard. Recognition errors must lean towards accepting.
- A wrong word shown as right, or an AI-invented "solution".
- Anything that needs an e-mail address or account, or stores more than the lists and their code.
- Klett content in the public repo or on a public URL.

## 4. Constraints

- Stick: M5StickC Plus2, 240x135, 2 MB PSRAM, internal heap is scarce (TLS needs it). All `.ino` files are one
  translation unit: prefix names with `vocab`. Firmware budget: stay under about 100 KB.
- Speech recognition: `gpt-transcribe` via OpenAI or the Zitro relay, today 8 kHz, 5 s, no language, no prompt.
- Spike (2026-11-13, macOS voices, 1014 transcriptions, scratchpad only) for spelling:
  - Misspelled words came back misspelled (1 of 288 "fixed", caused by a real word in the prompt).
  - English letter names, whole word correct: 58 % as the stick sends today, 88 % with 16 kHz +
    `language=en` + a spelling prompt. Single letters: 91 %.
  - German letter names: 22 to 48 %. Not usable. Children spell with English letter names.
  - "double p": 12 to 25 %, "double" is heard as W. Children say every letter.
  - Confusions: p/t, b/p, r/a (British "ar"), g/j.
  - Spelling "grandmother" with natural pauses takes up to 12 s.
- Text-to-speech: `gpt-4o-mini-tts` on `/v1/audio/speech`. A note in `ai_provider.h` says OpenAI shuts that
  endpoint down on 2027-01-06. The trainer's spoken words depend on it, as does all of Zitro's reading aloud.
  Not solved here; tracked as a risk.
- Server: cyon shared hosting, PHP 8.3 with zip, pdo_sqlite, sqlite3, mbstring; no zstd. Static files plus PHP,
  no database server needed. The repo is public: no server logins, no list content in commits.
- Copyright: Young World lists are "© Klett und Balmer, Kopiervorlage". Families may keep a private copy for
  their own use; Zitro must not publish or share them. Test files stay in the scratchpad.

## 5. Approaches considered

5.1 Linking lists to a stick
- **A. Secret code from the stick, shown as a QR code (chosen).** No account, keeps the "no account" promise.
  Losing the code means uploading again.
- B. Parent account with e-mail login. Multiple sticks per family, but breaks the promise and adds personal data.
- C. Upload on the stick's own setup page. No server, but the phone has no internet in the stick's hotspot, and
  the user wants zitro.spot to serve the lists.

5.2 Where files are parsed
- **Server, PHP (chosen by the user).** One parser, the stick gets clean JSON. New Anki exports are
  zstd-compressed and PHP on cyon has no zstd: such files get a clear message on how to export them.
- Browser (my first pick): instant preview, zstd possible in JS. Not chosen.

5.3 How the trainer runs on the stick
- **Own mode that takes over `loop()`, like the tour (chosen).** Button A must mean "answer".
- A result view like recipes: Button A means "new question" there. Rejected.

5.4 Public sources
- Quizlet: no open API found. AnkiWeb shared decks: no API, licences unstated. Klett: own paid packages
  (phase6). None can be offered lawfully, so none is offered. Parents can upload exports of their own sets.

## 6. Design

### 6.1 The stick's code and the QR screen

- On first need, the stick makes a 128-bit code with the hardware RNG (`esp_random`, WiFi on), base32
  (RFC 4648, no padding, 26 characters A-Z2-7), stored in Preferences namespace `voci`, key `code`. It never
  changes, except "new code" from the serial console for tests. A factory reset (Preferences cleared) makes a
  new one; the old lists expire on the server.
- Shown on the stick only as its last 3 characters, "Stick ••7KQ", so a parent can tell sticks apart.
- QR screen (same layout as the setup screen with the WiFi code): "Wörterlisten" / "Scanne den Code mit dem
  Handy:" / "zitro.spot/voci", QR on the right. QR content: `https://zitro.spot/voci/#<code>`. The code sits in
  the fragment, so it never reaches the server in the page request or its logs.
- Opened by voice ("Wörterlisten hochladen", "Voci hochladen", "upload vocabulary") through the answer field
  (6.4), and from the trainer when no list exists yet. Any button closes it.

### 6.2 Server: `webpage2/api/voci.php` and `webpage2/voci/`

Storage
- `webpage2/api/voci-data/` on the server, denied to the web by `.htaccess`, ignored by git.
- One directory per code, named `sha256(code)` in hex, so a leaked listing doesn't reveal codes.
- In it: `index.json` (lists: `id`, `book`, `name`, `lang`, `base`, `count`, `updated`), one `<id>.json` per list,
  `current` (the current book's name), `seen` (timestamp of the stick's last fetch).
- List file: `{"id","book","name","lang":"en","base":"de","entries":[{"t":"timetable","b":"Stundenplan, Fahrplan"}, ...]}`.
  `t` is the trained language (English), `b` the base language (German), both as in the file, trimmed,
  HTML stripped, whitespace folded.
- `id`: 8 lowercase hex characters, random, unique within the code.

Requests (all JSON; the code travels in header `X-Voci-Code`, never in the URL)
- `GET ?op=index` → `{"current": "...", "lists": [...]}`. Updates `seen` when the stick asks (User-Agent `Zitro`).
- `GET ?op=list&id=<id>` → the list file.
- `POST op=parse` (multipart `file`) → normalized entries plus guesses, nothing stored:
  `{"entries": [...], "book": "Young World 3", "name": "Unit 1", "swapped": false, "warnings": [...]}`.
- `POST op=save` (JSON: `book`, `name`, `entries`, optional `id` to replace) → stored after the same
  validation. The page sends back what `parse` returned, possibly with columns swapped or names changed.
- `POST op=rename` / `op=delete` / `op=current` / `op=wipe` (everything for this code).
- Errors: HTTP 4xx with `{"error": "<code>", "message": "<German text for parents>"}`.

Parsing (in `voci_lib.php`, pure functions, tested with PHP CLI)
- Encoding: UTF-8 with or without BOM, UTF-16 with BOM, otherwise Windows-1252 (Excel). Output UTF-8.
- Delimited text (`.csv`, `.tsv`, `.txt`): delimiter by count on the first lines: tab, then semicolon, then
  comma; quoted fields per RFC 4180. First two non-empty columns. A header row is dropped when both cells match
  known labels (en, english, englisch, de, deutsch, german, term, definition, word, wort, front, back, vorderseite,
  rückseite, case-insensitive).
- Quizlet text export: term and definition separated by tab (default) or comma, cards by newline (default) or
  semicolon. Covered by the delimited-text rule; a semicolon-separated single line with tabs inside is split
  into cards.
- Anki text export: lines starting with `#` are headers (`#separator:tab|comma|semicolon|pipe`, `#html:true`);
  then like delimited text. With `#html:true`, tags are stripped and entities decoded.
- Anki package (`.apkg`, `.colpkg`): zip; reads `collection.anki21`, else `collection.anki2` (SQLite), table
  `notes`, field `flds` split on `\x1f`, first two fields, HTML stripped, `[sound:...]` removed. Only
  `collection.anki21b` present (new zstd format): error telling the parent to export again with "Support older
  Anki versions" ticked (Anki's own wording; German UI: "Ältere Anki-Versionen unterstützen").
- Which column is which: the column with more German signals (ä ö ü ß, capitalised nouns, frequent German
  words) is the base. Default when unsure: first column trained language, second German. `swapped` tells the
  page; the parent can swap in the preview.
- Book and name from the file name: split at the last `unit|lektion|lesson|kapitel|chapter` + number, e.g.
  `young-world-3-unit-1.csv` → book "Young World 3", name "Unit 1". Otherwise book = file name, name = "Liste".
  Title-cased words, digits kept.
- Limits: upload up to 5 MB; up to 500 entries per list (more: rejected with a message); each side up to 120
  characters (longer entries skipped with a warning); entries with an empty side skipped with a warning;
  up to 100 lists and 1 MB of list files per code.

Abuse and privacy
- A valid-looking code creates its directory on the first `save`. New directories: at most 20 per IP hash per
  day (rate file in `voci-data/_rate/`, IP stored only as a salted hash, files older than two days removed).
- Writes: at most 120 per code per hour.
- Retention: on any request, with probability 1/50, directories whose `seen` and last write are both older
  than 365 days are deleted.
- No logging of codes or contents. The page states what is stored and offers "Alle Listen löschen".

Page `webpage2/voci/index.html` (+ `voci.js`, site `styles.css`)
- Reads the code from `location.hash`. Without one: a short explanation of how to get the QR code from the stick.
- Shows books (current first, with "★ aktuell"), units as chips with counts, rename and delete per list,
  "aktuell machen" per book.
- Upload: file input → `parse` → preview (first 5 rows, count, warnings, Buch and Name fields, "Spalten
  vertauschen") → `save`.
- German first, English when the browser language is not German. Same look as zitro.spot.

### 6.3 Stick: list lookup

- `vocabFetchIndex()` and `vocabFetchList(id)`: HTTPS GET to `https://zitro.spot/api/voci.php`, header
  `X-Voci-Code`, User-Agent `Zitro`, 10 s deadline, body limit 64 KB, parsed with ArduinoJson into PSRAM.
- Matching (pure, `vocab_core.h`): names normalized like recipes (lower case, letters and digits, one space).
  Number words "eins..zehn", "one..ten" become digits. Unit match: list name equals the query, or both contain
  the same number after a unit word. Book: if the AI gives a book, it must match a book name after
  normalization (or contain it); if not, the current book; if there is no current book and several books have
  the unit, the book chooser; one match opens directly.
- Nothing found: a screen "Keine Liste «Unit 7» gefunden" with the QR hint. No lists at all: the QR screen.

### 6.4 Answer field and prompt

- New field in the answer JSON: `vocab` (string, default empty), parsed in `ai_payload.ino` into
  `GPTResponse::vocab`. Values:
  - `upload` — show the QR screen.
  - `practice|<book>|<unit>`, `spell|<book>|<unit>`, `print|<book>|<unit>` — `<book>` may be empty.
- Prompt rule (German, ASCII, short): set `vocab` when the child wants to practise, learn, spell or print
  vocabulary ("Voci", "Wörtli", "Vokabeln", "Unit 3 üben", "buchstabieren"); copy book and unit as said; never
  supply words or translations. "Was heisst Hund auf Englisch?" stays a normal answer.
- `vocab` wins over `model`, images and Wolfram, like `is_tour`.

### 6.5 Trainer mode

Structure: `vocab_core.h` (pure logic: normalization, answer check, letter parsing, alignment, round queue,
name matching; host tests in `tests/vocab/run.sh`), `vocab.ino` (state, drawing, network, recording, printing
hand-off). `loop()` hands over to `vocabTick()` while `vocabActive()`, right after the timer blocks and next to
`guideTick()`; no other button handler runs.

Directions and modes
- `practice`: German shown, answer in English (default). Menu switches to "Englisch → Deutsch".
- `spell`: German shown, child spells the English word; only entries that are one word of letters (a-z, plus
  apostrophe and hyphen kept as visible fixed boxes) take part.
- `print`: goes straight to the printer list with the sheet.

Round
- 10 cards. First the entries answered wrong last time (stored per list), then the next ones from a cursor
  that walks the list (stored per list), so every word comes up over a few rounds.
- A wrong card goes to the end of the round once more. The score counts first attempts.
- Stored in Preferences namespace `voci`: per list id `w<id>` (indices wrong last round, up to 10 as bytes)
  and `c<id>` (cursor). Up to 50 lists remembered; the oldest entries are dropped.

Screens (see the mock): start (book, unit, direction, "Drück M5 zum Starten"), card (user, 2026-11-13: no
text besides a header and the word: top line = the C tab "?" then "Unit 1" and the progress dots; middle =
the A tab (microphone) beside the word, large, auto-shrunk to fit from 24 px down to 11 px, wrapped to two
lines if needed; bottom = the B tab; the direction only appears on the start screen; spelling adds the empty
letter boxes under the word), recording (boxes per second as today), result right (check, word large, base word small, speaker
mark while it is spoken), result wrong ("Ich habe gehört: «...»", "Richtig ist:", solution in citrus, "kommt
nochmal"), spelling boxes, filling, spelling result (green right, red wrong, orange missing, "Du: t-i-m-...",
one line saying what is off), end ("8 von 10", "2 Wörter üben wir nochmal"), menu (Weiter, Englisch →
Deutsch, Buchstabieren / Wörter sagen, Voci-Blatt drucken, Beenden; 3 visible rows with a scroll bar like the
printer list), book chooser (like the printer list).

Buttons
- Card: A answer (records), C "weiss nicht" (counts as wrong, shows the solution), B menu.
- Result: C or A next card, B menu. End: A next round, B menu.
- Menu and book chooser: C next row, A choose, B close. Menu preselects "Weiter".
- Button tabs stay visible in the trainer (as in the tour), because A means something else here.
- Recording: as today, one tap restarts, a second quick tap cancels and returns to the card.
- Holding B does not reset WiFi and triple C does not open settings while the trainer runs.

Checking spoken answers (deterministic first)
- Transcript and every solution are normalized: lower case; curly quotes and apostrophes unified; punctuation
  removed; text in parentheses removed ("(pl.)", "(pl. families)"); leading "to " removed for both; one space.
- Solutions: the list side split at ",", ";" and " / ". A slash between two single words inside a phrase gives
  one variant per word ("to the left/right of" → "to the left of", "to the right of"). Each variant, and the
  whole side, is a solution.
- Right if the answer equals a solution, or equals it with a leading article ("a", "an", "the") added or
  removed, or (for entries with "…") the answer starts with the part before "…" and ends with the part after
  it, with at least one word in between.
- Only if that fails and the entry contains "…" or has three or more words: one AI call with JSON mode asks
  whether the child's sentence says the same thing as the list sentence. The shown solution stays the list's.
  If the AI call fails, the answer counts as wrong but the screen says "Ich bin nicht sicher" instead of "Noch
  nicht" and does not store it as wrong.
- English → German direction: the same rules on the German side; capitalisation ignored.

Recording for answers
- Practice: the user's sample rate and recording time, plus `language=en` (or `de` for English → German).
  The spike showed `language=en` helps a little and never hurt.
- Spelling: 16 kHz into PSRAM (`ps_malloc`, 16 000 x 15 s x 2 bytes = 480 KB, freed after), `language=en`,
  prompt "A child spells an English word letter by letter, for example: Q, R, S, T." (no real word, because a
  real word in the prompt pulled one transcript towards it). Ends 1.2 s after the speech stops (level below a
  threshold measured in the first 0.3 s), at least 1.5 s, at most 15 s. The boxes on screen count seconds.
- `transcribeAudio()` gets parameters (sample count, rate, language, prompt) instead of reading globals only.
  Requests through the Zitro relay carry the same fields.

Letters from a spelling transcript
- Tokens split on spaces, commas, dots and hyphens. Single letters, and English letter names ("see", "aitch",
  "double you", "zed", "zee", ...), become letters. "double x" becomes "xx", but "double u"/"double you" is W.
  Anything else (a whole word) means the child said the word instead of spelling it: the result screen says
  "Bitte Buchstabe für Buchstabe" and the card stays.
- Comparison by edit distance alignment: each letter of the solution is right, wrong or missing; extra letters
  are shown as red boxes between. Sound-alike pairs count as right: b/p, p/t, d/t, b/d, g/j, m/n. Over-accepting
  is the chosen trade-off (anti-goal: never punish a recognition error).

Speaking the word
- `speakText()` gets an optional instruction; trainer words use "Say this English word clearly in British
  English, slowly, for a child learning English." (Young World is British English: maths, jumper, colour.)
  German solutions in English → German use the normal instruction. On failure the screen just stays silent.

Sleep and power
- A press keeps the stick awake for 5 minutes (as the tour and recipes); after that the normal timers.
- Light sleep keeps the trainer; a press on a dark screen only wakes and redraws.
- Deep sleep ends the trainer; waking shows the home screen.

Printing the sheet
- Menu "Voci-Blatt drucken" (or `vocab: print|...`): printer list, then print straight away (no size list).
- A4 pages rendered on the stick with the existing PWG raster code: title "Young World 3 · Unit 1", "Name" and
  "Datum" lines, two columns: German on the left, a writing line on the right; 18 rows per page. Then the
  same pages with the English filled in, titled "Lösungen". Footer per the print rules, credit "Wörterliste:
  eigene Datei" / "Word list: your own file".
- All entries of the list, in list order. Long entries wrap to two lines.
- One job per page if multi-page PWG jobs aren't supported by the current print code; the builder checks.

Serial commands (tests and screenshots)
- `vocab <value>` (as the answer field would), `vocabkey a|b|c`, `vocabtype <text>` (stands in for the next
  recording's transcript), `vocabcode` (prints the code), `vocabnewcode`.

### 6.6 What existing behaviour changes

- Button A in the trainer answers instead of asking a new question (like the tour).
- Spelling records at 16 kHz and up to 15 s regardless of the settings; normal questions are unchanged.
- `transcribeAudio()` and `speakText()` take optional parameters; existing calls behave as before.
- The answer JSON gets a `vocab` field; the prompt grows by one rule.

## 7. Testing

- `tests/vocab/run.sh` (host, g++): normalization, solutions and variants, article and "…" rules, letter
  parsing (including the spike's real transcripts as fixtures), alignment and marks, sound-alike pairs, round
  queue with stored wrongs and cursor, name and book matching, number words.
- `tests/voci-server/run.sh` (PHP CLI): each format with synthetic fixtures written for the test (no
  publisher content), encodings, header detection, column guess, file-name guess, limits, apkg legacy and
  zstd-only cases (fixtures built by the test script with PHP's zip and SQLite), API flow against `php -S`
  with a temporary data directory: parse, save, index, list, rename, current, delete, wipe, rate limit, bad code.
- `./verify.sh` compiles the firmware.
- End to end with the Young World 2 to 4 test files: uploaded under a test code to a local `php -S` server,
  then on the stick against zitro.spot after the user agrees to deploy.
- Real screenshots from the stick with `shot`, once flashed.

## 8. Documentation

- `docs/features/vocab.md`: requirements, user guide part, release note, sources (none shipped), privacy.
- `REQUIREMENTS.md`: a "Vocabulary trainer" section pointing to it, the changed behaviour in 6.6, and the
  reversed quiz decision.
- `USERGUIDE.md`, `RELEASE_NOTES.md` (end-user language, German and English as the files use).

## 9. Risks

- Text-to-speech endpoint shutdown on 2027-01-06 (see 4).
- Children's voices and the stick's microphone are worse than the spike's computer voices. The lenient rules
  and "not sure" states are there for that; real tests with a child decide whether spelling stays.
- A family that loses the stick's code loses access to its lists (they expire after a year).

## 10. Left to the builder

- Exact thresholds for the silence end, after measuring on the stick.
- Whether multi-page PWG jobs work with the existing print code.
- File and function split inside `vocab.ino` and `voci_lib.php`, as long as pure logic stays testable.
- Wording of on-screen texts within the mock's style, German and English.
