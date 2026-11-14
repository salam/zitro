# Vocabulary Trainer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Parents upload word lists to zitro.spot under their stick's code; children practise, spell and print a unit on the stick.

**Architecture:** A PHP endpoint (`webpage2/api/voci.php` + `voci_lib.php`) parses uploads and stores lists per hashed stick code; a static page (`webpage2/voci/`) uploads and manages them. On the stick, `vocab_core.h` holds the pure logic (checking, spelling, rounds, matching) and `vocab.ino` a takeover mode like the guided tour, started by a new answer field `vocab`.

**Tech Stack:** Arduino ESP32 (M5Unified/M5GFX, ArduinoJson), PHP 8.3 (zip, pdo_sqlite, mbstring), plain HTML/JS, host tests with c++17 and PHP CLI.

**Spec:** `docs/superpowers/specs/2026-11-13-vocab-trainer-design.md`

## Global Constraints

- All `.ino` files are one translation unit: every static name and constant starts with `vocab` (firmware) / `voci_` (PHP).
- Firmware growth under about 100 KB. Large buffers with `ps_malloc`, freed when the trainer ends.
- The right answer shown or spoken always comes from the list, never from the AI.
- No publisher content in the repo: fixtures are written for the tests. Young World test files stay in the scratchpad.
- The stick code travels only in header `X-Voci-Code` and the URL fragment, never in a query string; storage dirs are `sha256(code)`.
- Code format: 26 characters `A-Z2-7` (base32 of 16 random bytes).
- On-screen texts German and English (Preferences `settings/language`), umlauts only with `FontManager::getFont12pt()`.
- Commits: specific paths, message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. No push, no deploy, no flashing without the user's go.
- `./verify.sh` must pass after every firmware task.

## Review Focus

1. A list whose German side is in the first column (parent swapped columns): the column guess must flip it, and the preview must let the parent swap back. Test in Task 2.
2. Answers with typical speech-recognition noise: trailing period, capital letters, "The timetable.", "to the left of". Must count as right. Test in Task 1.
3. A child who says the word instead of spelling it ("timetable" instead of t-i-m-e...): must not count as a misspelling. Test in Task 1.
4. "Unit 2" when two books have a Unit 2 and none is current: must ask, not pick one. Test in Task 1.
5. An Excel CSV saved as Windows-1252 with semicolons ("Käse;cheese"): umlauts must survive. Test in Task 2.

---

### Task 1: Pure trainer logic `vocab_core.h`

**Files:**
- Create: `sketch/sketch_jan12a/vocab_core.h`
- Test: `tests/vocab/run.sh`, `tests/vocab/test_vocab.cpp`, `tests/vocab/fixtures/spike_transcripts.tsv` (the spike's English-letter transcripts with expected letters, copied from the scratchpad `detail.tsv`, word lists in it are the spike's own test words)

**Interfaces (Produces):**
```cpp
std::string vocabNormalize(const char* s);                 // lower case (ASCII and ÄÖÜ), ’‘`´ -> ', “” -> ", (...) removed,
                                                          // punctuation -> space, leading "to " removed, spaces folded
std::vector<std::string> vocabSolutions(const char* side); // normalized variants: split , ; " / ", word/word inside a phrase
enum VocabVerdict { VOCAB_RIGHT, VOCAB_WRONG, VOCAB_ASK_AI };
VocabVerdict vocabCheck(const char* answer, const char* side);
bool vocabSpellable(const char* side);                     // one word: letters, ' and - only
struct VocabLetters { std::string letters; bool spokeWord; };
VocabLetters vocabLettersFrom(const char* transcript);
enum VocabMark : uint8_t { VOCAB_OK, VOCAB_BAD, VOCAB_MISS, VOCAB_EXTRA };
struct VocabCell { char ch; VocabMark mark; };             // ch: the word's letter (OK/BAD/MISS) or the child's (EXTRA)
std::vector<VocabCell> vocabAlign(const char* word, const char* said);
bool vocabSoundAlike(char a, char b);                      // b/p p/t d/t b/d g/j m/n, both directions
bool vocabAllOk(const std::vector<VocabCell>& cells);
std::vector<uint16_t> vocabPlanRound(const std::vector<uint16_t>& lastWrong, uint16_t& cursor,
                                     const std::vector<bool>& eligible, size_t want);  // want = 10
std::string vocabNameKey(const char* s);                   // like recipes + number words (eins..zehn, one..ten) -> digits
struct VocabListRef { std::string id, book, name; };
enum VocabFindResult { VOCAB_FOUND, VOCAB_CHOOSE_BOOK, VOCAB_NOT_FOUND, VOCAB_NO_LISTS };
VocabFindResult vocabFind(const std::vector<VocabListRef>& lists, const char* currentBook, const char* book,
                          const char* unit, int& found, std::vector<std::string>& books);
struct VocabRequest { std::string mode, book, unit; };     // mode: upload | practice | spell | print
bool vocabParseRequest(const char* value, VocabRequest& out);
std::string vocabBase32(const uint8_t* bytes, size_t n);   // RFC 4648, no padding
```

- [ ] **Step 1: Write the failing tests** in `tests/vocab/test_vocab.cpp` (plain asserts with a counter, like `tests/guide`):
  - `vocabCheck("The timetable.", "timetable") == VOCAB_RIGHT`; `("time table", "timetable") == VOCAB_WRONG`;
    `("trousers", "trousers (pl.)") == VOCAB_RIGHT`; `("grade", "class/grade") == VOCAB_RIGHT`;
    `("to the right of", "to the left/right of") == VOCAB_RIGHT`; `("left of", "to the left/right of") == VOCAB_WRONG`;
    `("Rock", "Rock, Jupe") == VOCAB_RIGHT`; `("Jupe", "Rock, Jupe") == VOCAB_RIGHT`;
    `("How about pizza?", "How about …?") == VOCAB_RIGHT`; `("How about?", "How about …?") == VOCAB_ASK_AI`
    (a deterministic miss on a "…" entry asks the AI); `("What is with pizza", "How about …?") == VOCAB_ASK_AI`;
    `("story", "history") == VOCAB_WRONG`; `("That is boring", "That’s boring.") == VOCAB_ASK_AI` (3+ words);
    `("thats boring", "That’s boring.") == VOCAB_RIGHT`.
  - `vocabLettersFrom("C, H, E, E, S, E.").letters == "cheese"`; `("see aitch ee ee ess ee")` -> `"cheese"`;
    `("Double you, A, T")` -> `"wat"`; `("a double p l e")` -> `"apple"`; `("Cheese.").spokeWord == true`;
    every row of `fixtures/spike_transcripts.tsv` gives its expected letters.
  - `vocabAlign("timetable", "timtable")`: 9 cells, cell 3 `{'e', VOCAB_MISS}`, others OK;
    `("choir", "chior")`: cells 2 and 3 BAD; `("pupil", "pupill")`: 5 OK + one EXTRA `'l'`;
    `("bread", "pread")` all OK (sound-alike); `("jumper", "jumbe")` -> `vocabAllOk == false` (the r is missing).
  - `vocabPlanRound({4, 7}, cursor=0, eligible all true, 10)` on 30 entries -> `{4, 7, 0, 1, 2, 3, 5, 6, 8, 9}`, cursor 10;
    with cursor 28 it wraps: `{..., 28, 29, 0, ...}`; `eligible[1] == false` skips 1; fewer than 10 eligible -> all of them once.
  - `vocabFind`: lists YW2/3/4 x Unit 1..5; `(current "Young World 3", book "", unit "Unit 2")` -> FOUND YW3 Unit 2;
    `(current "", book "", unit "unit zwei")` -> CHOOSE_BOOK with 3 books; `(current "", book "young world 4", unit "2")` -> FOUND;
    `(unit "Unit 7")` -> NOT_FOUND; empty lists -> NO_LISTS; a single book with Unit 2 and no current -> FOUND.
  - `vocabParseRequest("practice||Unit 3")` -> mode practice, book "", unit "Unit 3"; `("upload")` -> mode upload; `("dance|x|y")` -> false.
  - `vocabBase32` of 16 zero bytes == `"AAAAAAAAAAAAAAAAAAAAAAAAAA"`.

- [ ] **Step 2: Run** `tests/vocab/run.sh` (copy `tests/recipe/run.sh`, sanitizers on). Expected: compile error (header missing).
- [ ] **Step 3: Implement `vocab_core.h`.** No Arduino includes; `std::string`/`std::vector` allowed (short lifetimes).
  "…" is UTF-8 `E2 80 A6`; treat "..." the same. `vocabCheck` order: exact solution; article added/removed (a, an, the);
  gap rule (starts with the part before "…", ends with the part after, at least one word between); else ASK_AI if
  the side contains "…" or its normalized form has 3+ words, else WRONG. Alignment: Levenshtein with substitution
  cost 0 for sound-alike pairs, back-trace preferring substitution.
- [ ] **Step 4: Run** `tests/vocab/run.sh`. Expected: all pass, no sanitizer report.
- [ ] **Step 5: Commit** `sketch/sketch_jan12a/vocab_core.h tests/vocab/`.

### Task 2: Upload parsing `voci_lib.php`

**Files:**
- Create: `webpage2/api/voci_lib.php`
- Test: `tests/voci-server/run.sh`, `tests/voci-server/test_lib.php` (builds its fixtures in a temp dir; apkg fixtures made with `ZipArchive` + `PDO sqlite`)

**Interfaces (Produces):**
```php
function voci_parse_file(string $path, string $filename): array;
  // ['entries' => [['t'=>..., 'b'=>...], ...], 'book' => 'Young World 3', 'name' => 'Unit 1',
  //  'swapped' => bool, 'warnings' => [string...]]   or throws VociError(code, German message)
function voci_guess_names(string $filename): array;   // ['book' => ..., 'name' => ...]
function voci_validate_entries(array $entries): array; // trims, folds, caps; returns [entries, warnings]; throws on > 500
const VOCI_MAX_ENTRIES = 500; const VOCI_MAX_SIDE = 120; const VOCI_MAX_UPLOAD = 5242880;
class VociError extends Exception { public string $errorCode; }
```

- [ ] **Step 1: Failing tests** (`test_lib.php`, asserts with exit code): UTF-8 CSV with header `en,de` -> header dropped,
  37 entries kept in order; TSV; semicolon CSV in Windows-1252 with "Käse;cheese" -> `b` = "Käse" and `swapped` true
  (German first); Quizlet export with tabs and newlines; Quizlet with commas and semicolons between cards; Anki text
  export with `#separator:tab` and `#html:true` (`<b>dog</b>` -> "dog", `&amp;` -> "&"); apkg with `collection.anki2`
  (notes `flds` "dog\x1fHund") -> one entry; apkg with only `collection.anki21b` -> VociError `anki_new_format`;
  empty right side -> skipped + warning; 501 rows -> VociError `too_many`; side of 130 chars -> skipped + warning;
  `voci_guess_names("young-world-3-unit-1.csv")` == Young World 3 / Unit 1;
  `("Lektion 4 Tiere.txt")` == `['book' => 'Tiere', 'name' => 'Lektion 4']` (unit word + number is the name, the rest
  is the book); `("animals.csv")` == `['book' => 'Animals', 'name' => 'Liste']`.
- [ ] **Step 2: Run** `tests/voci-server/run.sh`. Expected: FAIL (file missing).
- [ ] **Step 3: Implement.** Encoding detection order: BOM UTF-8/UTF-16, `mb_check_encoding(UTF-8)`, else Windows-1252.
  Delimiter by counting tab, `;`, `,` in the first 5 non-header lines. German score per column: umlauts/ß +2,
  capitalised first letter on a single word +1, hits in a short list of German function words (der die das und
  ist ein eine nicht mit) +1; higher score is `b`.
- [ ] **Step 4: Run.** Expected: PASS.
- [ ] **Step 5: Commit** `webpage2/api/voci_lib.php tests/voci-server/`.

### Task 3: Endpoint `voci.php` with storage, limits and retention

**Files:**
- Create: `webpage2/api/voci.php`, `webpage2/api/voci-data/.htaccess` (`Require all denied`)
- Modify: `webpage2/api/.gitignore` (ignore `voci-data/*` except `.htaccess`)
- Test: `tests/voci-server/test_api.sh` (starts `php -S 127.0.0.1:<port> -t webpage2` with env `VOCI_DATA_DIR` pointing to a temp dir)

**Interfaces:** requests and JSON shapes exactly as spec 6.2. `VOCI_DATA_DIR` env overrides the default `__DIR__/voci-data`.
Error codes: `bad_code`, `no_file`, `too_big`, `too_many`, `anki_new_format`, `unreadable`, `not_found`, `rate_limited`, `quota`.

- [ ] **Step 1: Failing API test:** bad code (25 chars) -> 400 `bad_code`; `parse` returns entries without storing
  (index still empty); `save` -> index has 1 list with `count`; `list&id` returns entries; `rename`; `current`;
  `delete`; `wipe` removes the directory; the 21st new code from one IP in a day -> 429 `rate_limited`; 101st list -> `quota`;
  code passed as `?code=` instead of the header -> 400 `bad_code`; the data dir contains `sha256(code)`, not the code.
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.** File locking with `flock` on `index.json`. `seen` updated on `op=index` when the
  User-Agent starts with `Zitro`. Retention sweep with `random_int(1, 50) === 1`. IP hash: `hash_hmac('sha256', ip, salt)`,
  salt in `voci-data/_salt` created on first use.
- [ ] **Step 4: Run** `tests/voci-server/run.sh` (runs lib and API tests). Expected: PASS.
- [ ] **Step 5: Commit.**

### Task 4: Page `zitro.spot/voci`

**Files:**
- Create: `webpage2/voci/index.html`, `webpage2/voci/voci.js`
- Test: manual against `php -S` with the Young World test files; a smoke check in `test_api.sh` that `/voci/` returns 200.

- [ ] **Step 1:** Page per spec 6.2 and the mock: code from `location.hash` (26 chars `A-Z2-7`, else the "how to get
  the QR code" text); books with current first, unit chips with counts, rename/delete/"aktuell machen"; upload ->
  preview (5 rows, count, warnings, Buch/Name fields, "Spalten vertauschen") -> save; "Alle Listen löschen" with an
  in-page confirmation. German, English when `navigator.language` doesn't start with `de`. Styles from `../styles.css`
  plus a small inline block; script tag `voci.js?v=1.0.0`.
- [ ] **Step 2:** Run `php -S`, open `/voci/#<test code>`, upload the 15 Young World test files, check counts
  (YW2: 31 32 30 33 33, YW3: 37 36 37 38 37, YW4: 40 x5).
- [ ] **Step 3: Commit.**

### Task 5: Firmware plumbing

**Files:**
- Modify: `sketch/sketch_jan12a/ai_provider.h` (`GPTResponse::vocab`), `ai_payload.ino` (parse `vocab`),
  `sketch_jan12a.ino` (system prompt field + rule; `transcribeAudio` and `recordAudio` parameters; `speakText`
  instruction; loop hook; guards; serial commands)

**Interfaces (Produces):**
```cpp
struct VocabRecordSpec { int sampleRate; int maxSeconds; bool stopOnSilence; };
bool recordAudioWith(const VocabRecordSpec& spec, int16_t*& buffer, int& samples, bool& cancelled); // PSRAM buffer, caller frees
String transcribeAudioWith(const int16_t* samples, int count, int sampleRate, const char* language, const char* prompt);
bool speakTextWith(const String& text, const char* instructions);   // speakText(text) == speakTextWith(text, default)
// in vocab.ino, declared near guideActive():
bool vocabActive(); void vocabTick(); void vocabStart(const String& request); void vocabSerial(const String& line);
```
- `transcribeAudio()` and `speakText()` keep their behaviour by calling the new functions with today's values.
- Prompt: add `\"vocab\": \"\"` to the JSON template and the rule from spec 6.4, in German ASCII, e.g.
  "VOKABELN: Will das Kind Woerter/Voci/Vokabeln ueben, lernen, buchstabieren oder ein Voci-Blatt drucken (z.B. 'Unit 3
  Englisch ueben', 'Young World 4 Unit 2 buchstabieren'), setze vocab auf 'practice|<Buch>|<Unit>' bzw. 'spell|...' oder
  'print|...' (Buch leer wenn nicht genannt). Will es Woerterlisten hochladen, setze vocab auf 'upload'. Nenne nie selbst
  Woerter oder Uebersetzungen. 'Was heisst Hund auf Englisch?' bleibt eine normale Antwort."
- Main flow: right after the `isTour` block, `if (gptResponse.vocab.length()) { vocabStart(gptResponse.vocab); return; }`.
- Loop: `if (vocabActive()) { vocabTick(); delay(20); return; }` next to the tour block; `btnCIsListButton` and the hold-B
  WiFi reset also check `vocabActive()`.
- Silence end: level = mean |sample| per 50 ms block; threshold = 3 x the first 0.3 s mean (min 200); stop after 1.2 s
  below threshold once speech was seen, never before 1.5 s, at most `maxSeconds`.

- [ ] **Step 1:** Implement, keeping existing calls unchanged.
- [ ] **Step 2: Run** `./verify.sh`. Expected: compiles.
- [ ] **Step 3: Commit.**

### Task 6: Trainer mode `vocab.ino`

**Files:**
- Create: `sketch/sketch_jan12a/vocab.ino`
- Consumes: Task 1 (all), Task 5 (record/transcribe/speak, hooks), `eduHttpGet`, `aiChat`, `drawPrintStatus`, printer list.

- [ ] **Step 1: Code and QR.** `vocabCode()` (Preferences `voci/code`, created with `esp_random` + `vocabBase32`);
  `vocabDrawQr()` with `canvas.qrcode(url, x, y, w, 4)`; serial `vocabcode`, `vocabnewcode`.
- [ ] **Step 2: Fetch and find.** `vocabFetchIndex()`, `vocabFetchList(id)` via `eduHttpGet` with header
  `X-Voci-Code` (extend `eduHttpGet` with an optional header only if it has no way to add one: then a local copy
  `vocabHttpGet`), JSON into PSRAM; `vocabFind` decides FOUND / book chooser / not found / QR screen.
- [ ] **Step 3: States and drawing** per the mock (version 2, card with header and word only): start, card, recording,
  checking, right, wrong, unsure, fill, spelling result, "Bitte Buchstabe für Buchstabe", end, menu, book chooser,
  not found, QR. Tabs always visible. Buttons per spec 6.5.
- [ ] **Step 4: Checking.** `vocabCheck`; on `VOCAB_ASK_AI` one `aiChat(system, user, 1024, false, true)` asking for
  `{"same": true|false}`; failure -> "Ich bin nicht sicher", not stored as wrong. Practice recordings use the user's
  rate/seconds with `language` en/de; spelling uses `{16000, 15, true}`, `language=en`, the spec's prompt.
- [ ] **Step 5: Rounds and memory.** `vocabPlanRound`; Preferences `voci` keys `w<id>` (bytes) and `c<id>` (uint16);
  at most 50 list ids kept (key `ids`, oldest dropped).
- [ ] **Step 6: Speaking.** After a right or wrong result, `speakTextWith(solution, british instruction)` for English,
  normal instruction for German.
- [ ] **Step 7: Sleep.** Keep awake 5 min after a press (`GUIDE_KEEP_AWAKE_MS` pattern), wake press only redraws,
  deep sleep ends the trainer.
- [ ] **Step 8: Serial:** `vocab <value>`, `vocabkey a|b|c`, `vocabtype <text>`.
- [ ] **Step 9: Run** `./verify.sh` and `tests/vocab/run.sh`. Expected: compiles, tests pass.
- [ ] **Step 10: Commit.**

### Task 7: The printed sheet

**Files:**
- Modify: `sketch/sketch_jan12a/print_layout.ino`, `pwg_raster.h` (`PWG_MAX_BITMAPS` 4 -> 8 if needed), `sketch_jan12a.ino`
  (`printImage()` and `printMenuConfirm()` honour a pending sheet)
- Test: `tests/pwg/run.sh` still passes; a sheet printed to `tests/pwg/fake_printer.py`.

**Interfaces:** `void vocabRequestSheet();` (vocab.ino: sets the pending sheet and opens the printer list),
`int vocabPrintSheet(PrinterInfo& printer);` (print_layout.ino, returns `PRINT_COMPOSE_*`), `bool vocabSheetPending;`.

- [ ] **Step 1:** With a sheet pending, `printMenuConfirm()` prints right after the printer step (no size list) and
  `printImage()` calls `vocabPrintSheet()`.
- [ ] **Step 2:** Pages per spec 6.5: 18 rows, title, Name/Datum, German left, writing line or English right, footer
  with credit "Wörterliste: eigene Datei" / "Word list: your own file"; exercise pages then "Lösungen" pages, one job
  per page; the same resolution plan as pictures.
- [ ] **Step 3: Run** `./verify.sh`, `tests/pwg/run.sh`, and `printtest`-style serial run against the fake printer.
- [ ] **Step 4: Commit.**

### Task 8: Documentation

**Files:** Create `docs/features/vocab.md`; modify `REQUIREMENTS.md`, `USERGUIDE.md`, `RELEASE_NOTES.md`.

- [ ] **Step 1:** `docs/features/vocab.md` with Requirements (spec 6, condensed), User guide, Release note, Privacy, Sources
  (none shipped; formats), Spike results. REQUIREMENTS: link in the 3D-view list style, the changed behaviour (spec 6.6),
  the reversed quiz decision. USERGUIDE: how parents upload, how children practise and spell, the sheet.
  RELEASE_NOTES: new dated section.
- [ ] **Step 2: Commit.**

### Task 9: End to end and screenshots (needs the user's go for deploy and flashing)

- [ ] **Step 1:** Local: `php -S`, test code, upload the 15 test files, `vocab practice|Young World 3|Unit 1` against a
  stick pointed at the local server (`modelhost`-style override) if available, else after deploy.
- [ ] **Step 2:** Ask the user before uploading `webpage2/api/voci*.php`, `webpage2/voci/` to zitro.spot and before flashing.
- [ ] **Step 3:** Real screenshots with `shot` for card, right, wrong, spelling, end, menu, QR.
