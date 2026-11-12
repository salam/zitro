# Dinosaur Fact Sheets and History of Life Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Naming a dinosaur (or pterosaur, marine reptile, early bird) shows a sourced fact sheet with pages for the picture, the facts, a size comparison and a fun fact. A new timeline, "History of life", runs from the first cells to *Homo sapiens*.

**Architecture:**
- **Prepared data:** Python generators read Wikidata, Wikipedia and Commons and write files to `webpage2/data/dino/` and `webpage2/data/timeline/`, which are served from zitro.spot.
- **On the stick:** a new `EduFeature` of kind `dino` uses a header-only, host-tested core (`dino_core.h`), in the same way as the timeline and protein features.
- **Deep time:** the timeline core moves its years to 64 bits so it can reach billions of years.

**Tech Stack:**
- Arduino C++ for ESP32 (M5StickC Plus2), with M5GFX and TJpgDec.
- Python 3 with Pillow, using the `Client` from `scripts/make_timeline_data.py`.
- Host tests with `c++ -std=c++17` and `python3 -I`.

**Spec:** `docs/superpowers/specs/2026-11-12-dinosaur-factsheet-design.md`. The mock is at `docs/superpowers/mocks/2026-11-12-dino-factsheet-mock.html`.

**Parts:** Part A (timeline) and Part B (fact sheet) can each be shipped on their own. Task C uploads the data and tests on the stick, and needs the user's go-ahead.

## Global Constraints

**Branch and process**
- Work only in the worktree `.worktrees/dino-factsheet`, on branch `feature/dino-factsheet`, which is based on `origin/main`. Never use local `master`, because its history carries a leaked key.
- Every commit message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- `./verify.sh` must pass after any change to `sketch/`. `RELEASE_NOTES.md`, `USERGUIDE.md` and `REQUIREMENTS.md` stay up to date (see `CLAUDE.md`).

**Sources**
- No AI-written fact, number or fun fact appears on the sheet. Every value keeps its source: a Wikidata property, or a Wikipedia language, title, revision ID and sentence.
- Values that disagree are shown as a range from min to max, never as an average.
- Speed is always marked "geschätzt" or "estimated".
- Fun facts and speeds are chosen, translated and shortened by the building AI, and no person reviews them. The automatic-tier pictures are looked at by the building AI, and no person looks at them. `docs/features/dino.md` must state both plainly.
- Polite downloads only: use `Client` from `make_timeline_data.py`, which pauses 1 s, caches on disk and backs off on 429.

**Device text**
- The device font covers only U+0020–U+05FF. Ranges use the ASCII hyphen ("12-13 m"); the en dash "–" (U+2013) must never reach a device file.
- Footer credits are ASCII only, because the small font (`DejaVu9`) only knows ASCII.
- Fixed strings, DE / EN:
  - Notes under the name: `Urvogel` / `Early bird`, `Flugsaurier - kein Dinosaurier` / `Pterosaur - not a dinosaur`, `Meeresreptil - kein Dinosaurier` / `Marine reptile - not a dinosaur`.
  - Fun-fact page heading: `Wusstest du?` / `Did you know?`.
  - Size page heading: `So gross war er` / `How big it was`, or `So lang war er` / `How long it was` when only a line is drawn. The person's label: `Mensch` / `Person`.

**Rules on the stick**
- The person in the size comparison is 170 cm.
- A sheet is shown only if it has its own picture or at least 2 fact rows. Otherwise `fetch()` returns false and today's Wikipedia picture shows.
- Buttons as in the timeline: C goes to the next page, and after the last page to the written answer, starting at page 1 on the next visit. B goes to the previous page and stops at the first. A asks a new question. Tilt uses `timelineTiltUpdate` with `TIMELINE_TILT_SIGN`.
- The index, the sheet and the picture all have to arrive within one `EDU_LOOKUP_DEADLINE_MS` (10 s).

**Limits**
- The timeline generator's `MAX_EVENTS = 20` stays, so `life_history` has at most 20 events. The spec allows 24; the generator is stricter.

## Review Focus

These five things are likely to bite users, and none of the tasks' basic tests covers them. Each one gets a test in the task that owns the code.

1. **Names in the forms the AI sends:** "T-Rex", "T. rex", "Tyrannosaurus Rex", "tyrannosaurus", or "Velociraptor mongoliensis" (a species). They must reach the right genus or fall back to the picture, and never show another animal. Tested in B1 `testResolveForms` and B2 `test_index_aliases`.
2. **Cut or corrupt downloads:** an index or sheet cut mid-line, a sheet without `end`, a picture cut short, a wrong hash. A broken picture leaves the sheet without its picture. If the sheet is then no longer worth showing, the normal picture appears. Garbage is never drawn. Tested in B1 `testParseHostile` and `testImageChecks`.
3. **Extreme sizes:** 80 cm (smaller than the person), 3500 cm, `lo == hi`, height greater than length, wingspan only (pterosaurs), no height. The layout must stay inside its area, and the person is always at least 2 px tall. Tested in B1 `testSizeLayout`.
4. **Text in one language only:** a meaning, diet or fun fact that exists only in DE or only in EN. The generator writes both, using the other language where one is missing, so the device never shows an empty value. Tested in B2 `test_sheet_text_languages`.
5. **Characters outside the device font:** Wikidata labels, aliases and Commons credits with an en dash, Greek letters or "ö" in a credit. The generator refuses them or folds them to ASCII. Tested on every generated file in B1 `testGeneratedFiles` and in B2 `test_sheet_text_refuses`.

## File Structure

| File | Responsibility |
| --- | --- |
| `sketch/sketch_jan12a/timeline_core.h` (modify) | 64-bit years, billions formatting |
| `sketch/sketch_jan12a/feature_timeline.ino` (modify) | year variables to 64-bit |
| `scripts/make_timeline_data.py` (modify) | Wikipedia-dated events, coarse precision, image size parameters |
| `scripts/make_timeline_allowlist.py` (modify) | the `life_history` topic |
| `tests/timeline/test_make_timeline_data.py` (create) | generator unit tests |
| `sketch/sketch_jan12a/dino_core.h` (create) | index lookup, sheet parsing, rows, pages, size layout; no Arduino includes |
| `sketch/sketch_jan12a/feature_dino.ino` (create) | fetch, draw, buttons, prompt rule |
| `scripts/make_dino_data.py` (create) | taxa, facts, pictures, index, sheets, credits |
| `scripts/make_dino_curated.py` (create) | clades and the curated tier, as a Python literal |
| `scripts/make_dino_rejects.py`, `scripts/make_dino_pins.py` (create) | AI picture review results; SHA-1 pins |
| `tests/dino/run.sh`, `tests/dino/test_dino.cpp`, `tests/dino/test_decode.cpp`, `tests/dino/test_make_dino_data.py` (create) | host tests |
| `webpage2/data/dino/` (generated) | `index-<pp>.txt`, `<slug>.txt`, `<slug>.jpg`, `credits.html` |
| `docs/features/dino.md` (create), `docs/features/timeline.md` (modify) | feature specs |

---

### Task 0: Put the design on the branch

**Files:** `docs/superpowers/specs/2026-11-12-dinosaur-factsheet-design.md`, `docs/superpowers/mocks/2026-11-12-dino-factsheet-mock.html` and this plan. They are already in the worktree.

- [ ] **Step 1: Commit**

```bash
git add docs/superpowers/specs/2026-11-12-dinosaur-factsheet-design.md docs/superpowers/mocks/2026-11-12-dino-factsheet-mock.html docs/superpowers/plans/2026-11-12-dino-factsheet.md
git commit -m "Design and plan for dinosaur fact sheets and the history of life"
```

## Part A: Timeline "History of life"

### Task A1: Deep time in the timeline core

**Files:**
- Modify: `sketch/sketch_jan12a/timeline_core.h`
- Modify: `sketch/sketch_jan12a/feature_timeline.ino`
- Test: `tests/timeline/test_timeline.cpp`

**Interfaces:**
- Produces:
  - `TimelineEvent::year` and `TimelineEvent::endYear` become `int64_t`.
  - `static const int64_t TIMELINE_YEAR_LIMIT = 5000000000LL;`
  - `timelineFormatYear(int64_t year, int64_t endYear, ...)`, `timelineFormatSpan(int64_t fromYear, int64_t toYear, ...)`, `timelineFormatMillions(int64_t, ...)`
  - `int64_t timelineYearAt(...)`
  - The file format `ZTL1` is unchanged: numbers simply get larger. The existing 25 files stay valid and are not regenerated.

- [ ] **Step 1: Write the failing test.** Add `testDeepTime()` to `test_timeline.cpp` and call it from `main`.

```cpp
static void testDeepTime() {
  char big[32], small[32], span[32];
  timelineFormatYear(-3500000000LL, 0, true, false, false, big, sizeof(big), small, sizeof(small));
  CHECK_STR(big, "ca. 3,5");
  CHECK_STR(small, "Mrd. Jahre her");
  timelineFormatYear(-2450000000LL, 0, false, false, true, big, sizeof(big), small, sizeof(small));
  CHECK_STR(big, "2.45");
  CHECK_STR(small, "billion years ago");
  timelineFormatYear(-541000000, 0, false, false, false, big, sizeof(big), small, sizeof(small));
  CHECK_STR(big, "541");
  CHECK_STR(small, "Mio. Jahre her");
  timelineFormatSpan(-3500000000LL, -2450000000LL, false, span, sizeof(span));
  CHECK_STR(span, "1,05 Mrd. Jahre");
  timelineFormatSpan(-3500000000LL, -2450000000LL, true, span, sizeof(span));
  CHECK_STR(span, "1.05 billion years");
  Timeline t;
  std::vector<char> s;
  CHECK(parseText("ZTL1\tdeep\tQ1\tT\tT\t1\t0\nQ2\t-3500000000\t-\t1\tA\tA\t0\t0\t0\t0\t-\t-\n", "deep", t, s));
  CHECK(t.events[0].year == -3500000000LL);
  CHECK(!parseText("ZTL1\tdeep\tQ1\tT\tT\t1\t0\nQ2\t-5000000001\t-\t1\tA\tA\t0\t0\t0\t0\t-\t-\n", "deep", t, s));
}
```

- [ ] **Step 2: Run it and see it fail.** Run `tests/timeline/run.sh`. Expected: a compile error or FAIL lines in `testDeepTime`.

- [ ] **Step 3: Implement.**
  - Every year variable in `timeline_core.h` and `feature_timeline.ino` becomes `int64_t`, and the `printf` calls use `%lld`.
  - The parser accepts years in ±`TIMELINE_YEAR_LIMIT`.
  - From |year| ≥ 1e9, numbers are written in billions with up to two decimals and trailing zeros trimmed, using a comma in DE and a point in EN. The words are "Mrd. Jahre her" / "billion years ago", and "Mrd. Jahre" / "billion years" in spans.
  - If an event's start is in billions, its end is written in billions too.
  - Below 1e9, everything stays exactly as it is.

- [ ] **Step 4: Run all the timeline tests.** Run `tests/timeline/run.sh`. Expected: `testDeepTime` and every existing check pass, including the generated-files and decode checks.

- [ ] **Step 5: Compile the sketch.** Run `./verify.sh`. Expected: it compiles.

- [ ] **Step 6: Commit.** Message: `Let timelines reach billions of years back`.

### Task A2: Wikipedia-dated events and coarse precision in the generator

**Files:**
- Modify: `scripts/make_timeline_data.py`
- Create: `tests/timeline/test_make_timeline_data.py`, in the style of `tests/protein/test_make_protein_data.py` (it imports the script through `sys.path`)
- Modify: `tests/timeline/run.sh`, adding `python3 -I test_make_timeline_data.py`

**Interfaces:**
- Produces:
  - `YEAR_LIMIT = 5_000_000_000`. It replaces the `±2**31` check in `date_of`.
  - `wikitext_plain(text: str) -> str` removes refs, templates and link markup, turns `{{convert|a|-|b|unit}}` into `a-b unit`, and collapses whitespace.
  - `check_date_sentence(sentence: str, year: int, revision_plain: str) -> str | None` returns a reason, or None when the sentence checks out.
  - `revision_plain(client, lang: str, title: str, revid: int) -> str | None` fetches one revision with `prop=revisions&revids=…&rvprop=content&rvslots=main&formatversion=2` and returns `wikitext_plain` of it. It returns None if the page title isn't `title`. B4 reuses it.
  - `wikipedia_date(client, source: dict) -> tuple[int, bool] | str` uses `revision_plain`, applies `check_date_sentence`, and returns `(source["year"], True)`, which is always circa.
  - New allow-list fields:
    - per item: `"wikipedia_date": {"lang", "title", "revid", "sentence", "year"}`
    - per topic: `"min_precision": 1` and `"coarse_circa": True`, which sets circa when the Wikidata precision is below 3
    - per topic, optional: `"title_de"` and `"title_en"`, which override the titles in the header

- [ ] **Step 1: Write the failing tests.**

```python
check(m.check_date_sentence("Life arose about 3.5 billion years ago.", -3500000000,
                            "x Life arose about 3.5 billion years ago. y") is None, "billion, EN")
check(m.check_date_sentence("Vor etwa 3,5 Milliarden Jahren entstand das Leben.", -3500000000,
                            "Vor etwa 3,5 Milliarden Jahren entstand das Leben.") is None, "Milliarden, DE")
check(m.check_date_sentence("Homo sapiens appeared about 300,000 years ago.", -300000,
                            "Homo sapiens appeared about 300,000 years ago.") is None, "thousands with comma")
check("number" in m.check_date_sentence("Homo sapiens appeared about 300,000 years ago.", -200000,
                                        "Homo sapiens appeared about 300,000 years ago."), "wrong number refused")
check("revision" in m.check_date_sentence("Not there.", -300000, "Something else."), "missing sentence refused")
check(m.wikitext_plain("a {{convert|15.3|–|17.9|km/h|mph}} b<ref>x</ref> [[Link|text]]") == "a 15.3-17.9 km/h b text",
      "wikitext cleanup")
ent = {"claims": {"P580": [{"rank": "normal", "mainsnak": {"snaktype": "value", "datavalue": {"value":
       {"time": "-2700000000-00-00T00:00:00Z", "precision": 1}}}}]}}
check(m.date_of(ent, 1, ["P580"])[0] == -2700000000, "years beyond 32 bits")
check(isinstance(m.date_of(ent, 3, ["P580"]), str), "too coarse for min_precision 3")
```

- [ ] **Step 2: Run them and see them fail.** Run `cd tests/timeline && python3 -I test_make_timeline_data.py`. Expected: AttributeError for `check_date_sentence`.

- [ ] **Step 3: Implement the functions above.**
  - Number matching in `check_date_sentence` reads a number with its scale word: billion / Milliarde(n) / Mrd. = 1e9, million / Million(en) / Mio. = 1e6, thousand / Tausend = 1e3, none = 1. It accepts `,` or `.` as the decimal or thousands separator, and the result must equal `abs(year)`.
  - `resolve_topic` uses `wikipedia_date` when an item has one, and Wikidata otherwise. The `expect` cross-check stays.
  - The topic's `title_de` and `title_en` win over the subject's labels.

- [ ] **Step 4: Run the tests and see them pass.** Run the same command. Expected: no FAIL lines, exit status 0.

- [ ] **Step 5: Check the existing timelines did not change.** Run `/opt/homebrew/bin/python3 -I scripts/make_timeline_data.py --cache <scratch>/tl --dates-only`. Expected: the same dates as before for all 25 existing topics.

- [ ] **Step 6: Commit.** Message: `Let deep-time timelines take sourced dates from Wikipedia`.

### Task A3: The `life_history` topic

**Files:**
- Modify: `scripts/make_timeline_allowlist.py`, `scripts/make_timeline_pins.py` (via `--pin`), `tests/timeline/test_timeline.cpp` (`testTopics`), `docs/features/timeline.md`, `USERGUIDE.md`, `RELEASE_NOTES.md`, `REQUIREMENTS.md`
- Generated: `webpage2/data/timeline/life_history.txt` and `.jpgs`, `credits.html`, `sketch/sketch_jan12a/timeline_topics.h`

**Interfaces:**
- Consumes: A1 (64-bit years) and A2 (allow-list fields).
- Produces: the topic id `life_history`. The prompt rule picks it up through `TIMELINE_TOPIC_LIST`.

- [ ] **Step 1: Update the topic test.** In `testTopics`, expect 26 topics, and expect `timelineIsTopic("life_history")` to be true. Run `tests/timeline/run.sh`. Expected: FAIL.

- [ ] **Step 2: Add the topic to the allow-list.**
  - Settings: id `'life_history'`, subject `'Life'`, `title_de: 'Geschichte des Lebens'`, `title_en: 'History of life'`, `min_precision: 1`, `coarse_circa: True`.
  - Events: at most 20, from the first cells to *Homo sapiens*. Candidates are in the spec, "Timeline 'History of life'".
  - Each event without a Wikidata date carries a `wikipedia_date` with a sentence copied from the live article and its revision ID.

- [ ] **Step 3: Check the dates.** Run `... make_timeline_data.py --cache <scratch>/tl --dates-only`. Expected: no REJECT for `life_history`, and the first event is about 3.5–4 billion years ago.

- [ ] **Step 4: Choose the pictures.**
  - Run `--candidates life_history`, then pick one Commons file per event, with its tag and fit.
  - Child safety exactly as in `timeline.md`: reconstructions, skulls, the Lucy skeleton and fossils are fine; nudity is not.

- [ ] **Step 5: Look at every picture.** Build, then run `--contact-sheets <scratch>/sheets` and Read every sheet image. Replace any unsuitable picture, then run `--pin` and build again.

- [ ] **Step 6: Run the tests.** Run `tests/timeline/run.sh`. Expected: everything passes, including the label wrap and the decode of `life_history`.

- [ ] **Step 7: Compile the sketch.** Run `./verify.sh`. Expected: it compiles.

- [ ] **Step 8: Update the docs.**
  - `docs/features/timeline.md`: 26 topics; the Wikipedia-date rule with revision IDs ("approved by the user on 2026-11-09"); deep time and "Mrd."; picture licence counts.
  - `USERGUIDE.md`: add the topic to the list.
  - `RELEASE_NOTES.md`: one bullet.
  - `REQUIREMENTS.md`: the timeline line, if it names the topics.

- [ ] **Step 9: Commit.** Message: `Add the history of life as a timeline, from the first cells to us`.

## Part B: Dinosaur fact sheet

### Task B1: `dino_core.h`, the logic on the stick

**Files:**
- Create: `sketch/sketch_jan12a/dino_core.h`. It includes `name_match.h` and has no Arduino includes.
- Create: `tests/dino/test_dino.cpp`, using the `CHECK` and `CHECK_STR` macros copied from `tests/timeline/test_timeline.cpp`, and the real font `fonts/PrimaryFont10pt.h` for widths.
- Create: `tests/dino/run.sh`, modelled on `tests/timeline/run.sh`. It passes `../../webpage2/data/dino` to the test, and that folder may not exist yet.

**Interfaces:**
- Produces the file formats. All files are UTF-8, tab-separated and end with `\n`.

```
index-<pp>.txt  pp = first two characters of the normalised name, each [a-z0-9] or else "_"
  ZDX1\t<pp>\t<count>
  <normalised name>\t<slug>          sorted by name; slug matches [a-z0-9_]{1,40}

<slug>.txt      every key at most once, in any order; unknown keys are ignored; "end" must be last
  ZDS1\t<slug>\t<QID>
  name\t<genus>
  group\t<0 dinosaur | 1 early bird | 2 pterosaur | 3 marine reptile>
  image\t<bytes>\t<w>\t<h>\t<fnv1a hex>\t<credit or ->
  length\t<lo cm>\t<hi cm>         height, wingspan: the same, in cm
  mass\t<lo g>\t<hi g>             speed\t<lo>\t<hi> in 0.1 km/h      egg\t<lo mm>\t<hi mm>
  lived\t<older, years ago>\t<younger, years ago>
  named\t<year>
  era\t<de>\t<en>                  meaning, diet, found: the same
  fun\t<de>\t<en>\t<source, e.g. Wikipedia (EN): Maiasaura>
  source\t<footer text for the facts pages, e.g. Wikidata, Wikipedia>
  end
```

- Produces the constants: `DINO_INDEX_LIMIT 16384`, `DINO_TEXT_LIMIT 2048`, `DINO_IMAGE_LIMIT 32768`, `DINO_IMAGE_W 240`, `DINO_IMAGE_H 117` (the footer takes the bottom 18 px, `EDU_FOOTER_HEIGHT`), `DINO_FIELD_LIMIT 160` (bytes per text field), `DINO_PERSON_CM 170`, `DINO_ROWS_PER_PAGE 6`, `DINO_MAX_ROWS 12`, `DINO_MAX_PAGES 5`.
- Produces the types:
  - `enum DinoGroup : uint8_t {DINO_DINOSAUR, DINO_EARLY_BIRD, DINO_PTEROSAUR, DINO_MARINE}`
  - `struct DinoRange { int64_t lo, hi; bool present; }`
  - `struct DinoText { const char* de; const char* en; }`, where nullptr means absent
  - `struct DinoSheet` with: `slug, qid, name, group, hasImage, imageLength, imageW, imageH, imageHash, credit, length, height, wingspan, mass, speed, egg, lived` (each a `DinoRange`), `named` (an `int32_t`, 0 for none), `era, meaning, diet, found, fun` (each a `DinoText`), `funSource`, `source`
  - `struct DinoRow { char label[20]; char value[DINO_FIELD_LIMIT + 40]; }`
  - `enum DinoPage {DINO_PAGE_PICTURE, DINO_PAGE_FACTS, DINO_PAGE_FACTS_MORE, DINO_PAGE_SIZE, DINO_PAGE_FUN}`
  - `struct DinoSizeLayout { int personX, personY, personW, personH, bodyX, bodyY, bodyW, bodyH, rangeW; bool lineOnly, wingspan; }`
- Produces the functions:
  - `bool dinoIndexName(const char* query, char* normalised, size_t cap, char* indexFile, size_t fileCap)`. It uses `eduNormaliseName(query, …, false)`, and `indexFile` is `"index-<pp>.txt"`. A one-character name gets `_` as its second character.
  - `bool dinoResolve(char* index, size_t len, const char* prefix, const char* normalised, char* slug, size_t slugCap)`
  - `bool dinoParseSheet(char* text, size_t len, size_t capacity, const char* expectedSlug, DinoSheet& out)`. It works in place, like `timelineParse`.
  - `uint32_t dinoFnv1a(const uint8_t* data, size_t length)` and `bool dinoImageOk(const DinoSheet& s, const uint8_t* data, size_t have)`
  - `const char* dinoText(const DinoText& t, bool english)`, which falls back to the other language
  - `const char* dinoGroupNote(DinoGroup g, bool english)`, which returns "" for dinosaurs
  - `int dinoFactRows(const DinoSheet& s, bool english, DinoRow* rows, int max)`
  - `bool dinoWorthShowing(const DinoSheet& s)`
  - `int dinoPages(const DinoSheet& s, int rowCount, DinoPage* pages, int max)`
  - `bool dinoSizeLayout(const DinoSheet& s, int areaW, int areaH, DinoSizeLayout& out)`
  - `void dinoFormatLength(int64_t loCm, int64_t hiCm, bool english, char* out, size_t size)`, plus `dinoFormatMass`, `dinoFormatSpeed`, `dinoFormatEgg` and `dinoFormatLived`, each with the same shape

**Row order and labels (DE / EN):**

| Label | Value |
| --- | --- |
| Länge / Length | |
| Höhe / Height | |
| Spannweite / Wingspan | |
| Gewicht / Weight | |
| Tempo / Speed | `<range> km/h, geschätzt` / `<range> km/h, estimated` |
| Ei / Egg | |
| Zeit / Era | |
| Lebte / Lived | `vor 68-66 Mio. Jahren` / `68-66 million years ago` |
| Benannt / Named | |
| Name bedeutet / Name means | |
| Nahrung / Diet | |
| Fundort / Found in | |

Rows without a value are left out.

**Number formats:**
- Length: below 100 cm → `"80 cm"`. Otherwise metres with up to 2 decimals and trailing zeros trimmed: `"12-13 m"`, `"1,8-2,07 m"`, and `"12 m"` when lo == hi.
- Mass: below 1000 g → grams. Below 1 t → kg, with one decimal below 10 kg. From 1 t → tonnes with up to one decimal: `"8,8 t"`, `"15 kg"`.
- Speed: tenths of km/h, with `.0` trimmed: `"20-30 km/h"`, `"15,3-17,9 km/h"`.

**Pages:**
- The order is picture (only if `hasImage`), facts (only if there is at least 1 row), facts more (only for more than 6 rows), size (only if `dinoSizeLayout` is true), fun (only if `fun` is set).

**Size layout:**
- The scale is the smaller of `areaW / max(length.hi or wingspan.hi, DINO_PERSON_CM)` and `areaH / max(height.hi, DINO_PERSON_CM)`.
- If there is no height, the bar is a line 5 px high (`lineOnly`).
- If there is no length but there is a wingspan, the wingspan is drawn as the line (`wingspan`).
- If there is neither, the function returns false.

- [ ] **Step 1: Write the failing tests.** Add these to `test_dino.cpp`. The fixtures are inline strings. `TREX` is the sheet with the values from the mock: length 1200–1300, height 360–400, mass 8800000, speed 200–300, lived 68000000–66000000, era Kreide/Cretaceous, named 1905, a fun fact, and an image line.
  - `testIndexName`:
    - `"T-Rex"` gives `"t rex"` and `"index-t_.txt"`
    - `"Tyrannosaurus"` gives `"index-ty.txt"`
    - `"Ärgerosaurus"` gives `"aergerosaurus"` and `"index-ae.txt"`
    - `""` and `"-"` give false
  - `testResolveForms`:
    - the `ty` index with `tyrannosaurus` and `tyrannosaurus rex` → `tyrannosaurus` resolves both
    - the `t_` index with `t rex` and `t. rex` resolves `"T-Rex"` and `"T. rex"`
    - the `ve` index with `velociraptor mongoliensis` → `velociraptor` resolves the species
    - `"tyranno"` → false
    - an index whose header prefix is `xx` → false
    - a slug with `/` or `..` → false
  - `testParseGood`: `TREX` parses, `length.lo == 1200`, `named == 1905`, `dinoText(s.fun, true)` is the EN text, and `group == DINO_DINOSAUR`.
  - `testParseHostile`:
    - refused: no `end`; cut in the middle of a line; a wrong slug in the header; a duplicate key; `lo > hi`; a negative length; a `group` of 4; a text field over `DINO_FIELD_LIMIT`; a byte 0x01; a 3-byte UTF-8 character (`"\xE2\x80\x93"`); an image wider than 240
    - accepted, with the key ignored: an unknown key
  - `testImageChecks`: `dinoImageOk` is true for matching bytes, and false for a cut copy, a wrong hash, and data without `FF D8` at the start or `FF D9` at the end.
  - `testFormats`:
    - `dinoFormatLength(1200,1300,false)` is `"12-13 m"`; `(180,207,false)` is `"1,8-2,07 m"`; `(180,207,true)` is `"1.8-2.07 m"`; `(80,80,false)` is `"80 cm"`
    - `dinoFormatMass(8800000,8800000,false)` is `"8,8 t"`; `(15000,15000,true)` is `"15 kg"`
    - `dinoFormatSpeed(153,179,false)` is `"15,3-17,9 km/h"`
    - `dinoFormatLived(68000000,66000000,false)` is `"vor 68-66 Mio. Jahren"`
  - `testRows`: `TREX` in DE gives the rows in the order of the table above, the speed row reads `"20-30 km/h, geschätzt"`, and there is no Egg row.
  - `testWorthShowing`:
    - a picture and 0 rows → true
    - no picture and 2 rows → true
    - no picture and 1 row → false
    - a fun fact only → false
  - `testPages`:
    - `TREX` gives picture, facts, size, fun
    - with no image and no length: facts, fun
    - 8 rows: picture, facts, facts more, size
  - `testSizeLayout`, all with area 224×79:
    - T. rex: the person's height to the bar's height equals 170/400 within 1 px
    - all T. rex rectangles lie inside the area
    - 80 cm with height 50: the person is taller than the body
    - 3500 cm with no height: `lineOnly`, and the person is at least 2 px
    - wingspan only: `wingspan` is true
    - neither: false
    - lo == hi: `rangeW == 0`
  - `testGeneratedFiles(dir)`. If the folder exists, check:
    - every `index-*.txt` parses and is at most `DINO_INDEX_LIMIT` bytes; every sheet is at most `DINO_TEXT_LIMIT` and every picture at most `DINO_IMAGE_LIMIT`
    - every slug resolves to a `<slug>.txt` that parses
    - `dinoWorthShowing` is true for every sheet
    - every row value fits 224 px − label width in Roboto 10pt
    - every fun fact wraps into at most 5 lines of 224 px with the font that `feature_dino.ino` uses (Task B5 sets this; until then use Roboto 10pt)

- [ ] **Step 2: Run them and see them fail.** Run `tests/dino/run.sh`. Expected: a compile error (`dino_core.h` is missing).

- [ ] **Step 3: Implement `dino_core.h`** to the interfaces above.

- [ ] **Step 4: Run them and see them pass.** Run `tests/dino/run.sh`. Expected: `0 failures`. The generated-files part is skipped while the folder doesn't exist.

- [ ] **Step 5: Commit.** Message: `Add the fact sheet core: lookup, sheet parsing, pages and size comparison`.

### Task B2: Generator, automatic tier without pictures

**Files:**
- Create: `scripts/make_dino_data.py`. It inserts its own folder into `sys.path` and imports `make_timeline_data as tl` for `Client`, `wiki_entities`, `statements`, `wikidata_time`, `date_of`, `label_of`, `batches`, `fnv1a`, `licence_problem`, `device_credit` and `prepare_image`.
- Modify: `scripts/make_timeline_data.py`. `prepare_image(data, fit, width=IMAGE_W, height=IMAGE_H)` gains these keyword arguments, and its existing calls are unchanged.
- Create: `scripts/make_dino_curated.py`. It is a Python literal like the allow-list. In this task it holds `"clades"` and an empty `"curated": []`.
- Create: `tests/dino/test_make_dino_data.py`, and add `python3 -I test_make_dino_data.py` to `tests/dino/run.sh`.

**Interfaces:**
- Consumes: the B1 file formats.
- Produces:
  - `clades` uses English Wikipedia titles, which the generator resolves to QIDs and prints: `{"dinosaur": "Dinosaur", "early_bird": "Avialae", "modern_birds": "Neornithes", "pterosaur": "Pterosaur", "marine": ["Plesiosauria", "Ichthyosaur", "Mosasaur"]}`.
  - `classify(qid: str, members: dict[str, set[str]]) -> int | None`. The order is: in `modern_birds` → None; `early_bird` → 1; `dinosaur` → 0; `pterosaur` → 2; any `marine` → 3; otherwise None.
  - `UNITS: dict[str, tuple[str, float]]`, for example `"Q11573": ("length", 100)` (metre → cm), `"Q3710": ("length", 30.48)`, `"Q11570": ("mass", 1000)` (kg → g), `"Q191118": ("mass", 1e6)`, `"Q100995": ("mass", 453.59237)`. Add every other unit met in the data after checking it on its Wikidata item. An unknown unit drops the value with a log line.
  - `convert(amount: str, unit_qid: str, kind: str) -> int | None`, rounded to the integer unit.
  - `value_range(values: list[int]) -> tuple[int, int] | None`
  - `era_text(older: int, younger: int, periods: list[tuple[int, int, str, str]]) -> tuple[str, str] | None`. The periods are Permian Q76402, Triassic Q47158, Jurassic Q45805, Cretaceous Q44626 and Paleogene Q55810, dated through `tl.date_of`. A range inside one period gives `("Kreide", "Cretaceous")`; across two it gives `("Jura bis Kreide", "Jurassic to Cretaceous")`.
  - `normalise(name: str) -> str | None`, the same rules as `eduNormaliseName(…, false)`, copied from `make_protein_data.normalise`.
  - `slug_of(genus: str) -> str`
  - `index_files(entries: list[tuple[str, str]]) -> dict[str, str]`, keyed by the two-character prefix as in B1. A normalised name that points to two slugs is dropped and logged; a genus name wins over an alias. It raises `ValueError` for a file over 16384 bytes.
  - `LIMITS = {"index": 16384, "sheet": 2048, "picture": 32768}`. Every write is checked against them, and the generator stops on any excess. It empties `webpage2/data/dino/` before writing, so no stale sheet stays behind.
  - `sheet_text(sheet: dict) -> str`. It writes the B1 format and raises `ValueError` for any character outside U+0020–U+05FF and for a field over 160 bytes. A text missing in one language is filled from the other.
  - `worth_showing(sheet: dict) -> bool`, the same rule as `dinoWorthShowing`.
  - Data rules:
    - **Facts from Wikidata:** P2043 length, P2048 height, P2050 wingspan, P2067 mass; P580/P582 and P523/P524 for `lived`; the year from P225 qualifier P574 for `named`.
    - **Labels:** the genus name is P225. The aliases are the de/en labels and aliases of the genus and of its species items (P171 = genus, with SPARQL).

- [ ] **Step 1: Write the failing tests.**
  - `test_units`:
    - `convert("+18","Q3710","length") == 549`
    - `convert("+38000","Q100995","mass") == 17236510`
    - `convert("+12.0","Q11573","length") == 1200`
    - an unknown unit gives None
  - `test_ranges`: `value_range([1200,1300,1200]) == (1200,1300)` and `value_range([]) is None`.
  - `test_classify`: a QID in both `dinosaur` and `modern_birds` gives None; in `dinosaur` and `early_bird` it gives 1; in a `marine` set it gives 3.
  - `test_era`: 68–66 Ma gives `("Kreide","Cretaceous")`, and 150–140 Ma gives `("Jura bis Kreide","Jurassic to Cretaceous")`, using the real period boundaries passed in.
  - `test_normalise`: `"T-Rex"` gives `"t rex"`, `"Tyrannosaurus  Rex"` gives `"tyrannosaurus rex"`, and `"Ärger"` gives `"aerger"`.
  - `test_index_aliases`: `index_files([("t rex","tyrannosaurus"),("tyrannosaurus","tyrannosaurus"),("velociraptor mongoliensis","velociraptor")])` has the keys `"t_"`, `"ty"` and `"ve"`. A name pointing to two slugs is dropped, and 1000 long names with one prefix raise `ValueError`.
  - `test_sheet_text_refuses`: `sheet_text` raises for an en dash in `name`, and for a 161-byte meaning.
  - `test_sheet_text_languages`: a `meaning` with only `de` is written with the DE text in both columns.
  - `test_worth_showing`: the same four cases as B1 `testWorthShowing`.

- [ ] **Step 2: Run them and see them fail.** Run `cd tests/dino && python3 -I test_make_dino_data.py`. Expected: ImportError, or FAIL.

- [ ] **Step 3: Implement the functions and the pipeline.**
  - The pipeline: resolve the clades → one SPARQL query per clade root (`?t wdt:P171+ wd:<root>; wdt:P105 wd:Q34740`) → `wbgetentities` in batches of 50 → facts → `worth_showing` → write the index and sheets.
  - Command line: `--cache <dir>`, `--no-pictures`.
  - It prints counts per group, and every value it dropped, with the reason.

- [ ] **Step 4: Run the tests and see them pass.** Run the same command. Expected: no FAIL lines.

- [ ] **Step 5: Generate the data.** Run `/opt/homebrew/bin/python3 -I scripts/make_dino_data.py --cache <scratch>/dino --no-pictures`. Expected:
  - `webpage2/data/dino/index-*.txt` and the `<slug>.txt` files exist.
  - The printed counts are plausible: hundreds of dinosaur genera, tens of each other group.

- [ ] **Step 6: Run the host tests and check the data.** Run `tests/dino/run.sh`. Expected: `testGeneratedFiles` passes over every file. Then check by hand from the generated sheets:
  - `tyrannosaurus` has group 0, `archaeopteryx` group 1, `pteranodon` group 2 and `mosasaurus` group 3.
  - The modern bird genera `passer`, `columba` and `gallus` are in no index.
  - The dinosaur count is below 2000. A count in the thousands means modern birds slipped past the Neornithes exclusion.

- [ ] **Step 7: Commit.** Commit the generator, its tests and the generated data. Message: `Generate dinosaur fact sheets from Wikidata`.

### Task B3: Pictures for the automatic tier

**Files:**
- Modify: `scripts/make_dino_data.py`
- Create: `scripts/make_dino_rejects.py`, a literal `{"<QID>": "<reason>"}` written by the reviewing AI; `scripts/make_dino_pins.py`, written by `--pin`; `tests/dino/test_decode.cpp`, copied from `tests/timeline/test_decode.cpp` and adapted to single `.jpg` files
- Modify: `tests/dino/run.sh`, adding the decode step as in the timeline

**Interfaces:**
- Produces:
  - `DINO_CATEGORY_DENY`, a regex that extends `tl.CATEGORY_DENY` with `carcass|prey|predation|kill|killing|hunting|blood|wound|fight|attack|dead|death|scavenging`
  - `picture_problem(file_info: dict, rejects: dict, pins: dict, qid: str) -> str | None`
  - Output: `<slug>.jpg` at 240×117 at most, made with `tl.prepare_image(..., width=240, height=117)`, plus the `image` line and `credits.html`
  - Command line: `--contact-sheets <dir>` writes 48 pictures per sheet, captioned with the slug and QID; `--pin`

- [ ] **Step 1: Write the failing tests.** Add `test_picture_problem`. Each case returns a reason, except the last:
  - the category `"Paleoart of predation"`
  - the licence `"CC BY-NC 4.0"`
  - no artist
  - a QID in `rejects`
  - a SHA-1 different from the pin
  - a clean file with CC BY-SA 4.0 returns None

- [ ] **Step 2: Run them and see them fail.** Run `cd tests/dino && python3 -I test_make_dino_data.py`. Expected: FAIL.

- [ ] **Step 3: Implement `picture_problem`, the downloads, the sheets, `--contact-sheets` and `--pin`.** Run the generator with pictures. Expected: the number of pictures and the reasons for refusals are printed.

- [ ] **Step 4: Review the pictures.**
  - Run `--contact-sheets <scratch>/dino-sheets`, then Read every sheet image.
  - Put every picture that shows predation, a carcass, gore or anything else unsuitable for young children into `make_dino_rejects.py`, with a short reason.
  - Build again, then run `--pin`, then build again.

- [ ] **Step 5: Run the host tests.** Run `tests/dino/run.sh`. Expected: all pass, and every `.jpg` decodes with `lgfx_tjpgd`.

- [ ] **Step 6: Commit.** Message: `Give dinosaur fact sheets checked pictures`.

### Task B4: The curated tier, about 50 species

**Files:**
- Modify: `scripts/make_dino_curated.py`, `scripts/make_dino_data.py`, `tests/dino/test_make_dino_data.py`

**Interfaces:**
- Consumes: `tl.wikitext_plain` from A2, and the revision fetch from A2's `wikipedia_date`, factored out as `tl.revision_plain(client, lang, title, revid) -> str`.
- Produces the curated entry format:

```python
{"wiki": "Tyrannosaurus", "image": "<Commons file>", "tag": "fossil", "fit": "cover",
 "facts": [{"field": "speed", "quote": "20–30 km/h", "lang": "de", "title": "Tyrannosaurus", "revid": 272068446,
            "sentence": "Die maximale Geschwindigkeit ausgewachsener Tiere betrug etwa 20–30 km/h."},
           {"field": "meaning", "de": "Tyrannenechse", "en": "tyrant lizard", "lang": "en", "title": "Tyrannosaurus",
            "revid": 1381719568, "sentence": "..."}],
 "fun": {"de": "...", "en": "...", "lang": "en", "title": "Tyrannosaurus", "revid": 1381719568, "sentences": ["..."]}}
```

- Produces these functions:
  - `parse_quantity(quote: str) -> tuple[str, int, int] | None` returns `(kind, lo, hi)` in the B1 units. It accepts m / Meter(n) / metres / meters / cm / Zentimeter / t / Tonnen / tonnes / kg / Kilogramm / km/h, with `–`, `-` or `bis` between two numbers, and `,` or `.` as the decimal mark.
  - `check_fact(fact: dict, revision_plain: str) -> str | None`. The sentence must be in the revision and the quote in the sentence. A numeric field's `parse_quantity(quote)` must give the field's kind. For the field `named`, the quote is the four-digit year, as in the infobox's "Osborn, 1905".
  - `check_fun(fun: dict, revision_plain: str) -> str | None`. Every sentence must be in the revision. Every number written in `de` and `en` must appear in the sentences.
- Curated facts are merged with the Wikidata values into one range per field.

- [ ] **Step 1: Write the failing tests.**
  - `parse_quantity("20–30 km/h") == ("speed", 200, 300)`
  - `parse_quantity("bis zu 13 Metern")[1:] == (1300, 1300)`
  - `parse_quantity("15.3–17.9 km/h") == ("speed", 153, 179)`
  - `parse_quantity("8,8 Tonnen") == ("mass", 8800000, 8800000)`
  - `check_fact` refuses a quote that isn't in its sentence, and a sentence that isn't in the revision.
  - `check_fun` refuses a German text with "40" when the sentence says "30".

- [ ] **Step 2: Run them and see them fail.** Same command as before. Expected: FAIL.

- [ ] **Step 3: Implement** the three functions and the merge.

- [ ] **Step 4: Run the tests and see them pass.** Expected: no FAIL lines.

- [ ] **Step 5: Write the curated entries.**
  - About 50 genera: the ones children ask about most, every dinosaur in the existing timelines, plus Pteranodon, Quetzalcoatlus, Plesiosaurus, Mosasaurus, Ichthyosaurus and Archaeopteryx.
  - For each one, read the current de and en articles and copy the sentences together with their revision IDs.
  - Pick a hand-chosen picture, with child safety as in B3.
  - Speed and egg size go in only where a sentence ties them to this genus. A walking speed is not a running speed.

- [ ] **Step 6: Generate and run the tests.** Run the generator, Read the contact sheet of the curated pictures, run `--pin`, then run `tests/dino/run.sh`. Expected: all pass, and every curated fun fact fits within 5 lines.

- [ ] **Step 7: Commit.** Message: `Add hand-checked facts, speeds and fun facts for the best-known species`.

### Task B5: The view on the stick

**Files:**
- Create: `sketch/sketch_jan12a/feature_dino.ino`
- Modify: `sketch/sketch_jan12a/model_core.h` (add `"dino"` to `MODEL_KINDS`), `sketch/sketch_jan12a/edu_feature.h` (`FEATURE_DINO` in the extern list), `sketch/sketch_jan12a/feature_registry.ino` (`&FEATURE_DINO`), `sketch/sketch_jan12a/timeline_core.h` and `feature_timeline.ino` (move `TIMELINE_TILT_SIGN`), `sketch/sketch_jan12a/sketch_jan12a.ino` (the miss line below), `tests/model/test_model.cpp`

**Interfaces:**
- Consumes: everything from B1, and `timelineTiltUpdate`, `TimelineTilt` and `TIMELINE_TILT_SIGN` from the timeline. Arduino joins the `.ino` files in alphabetical order, so `feature_dino.ino` comes before `feature_timeline.ino`. It must `#include "timeline_core.h"` and `"dino_core.h"` itself, and `TIMELINE_TILT_SIGN` moves from `feature_timeline.ino` into `timeline_core.h`.
- Produces: `const EduFeature FEATURE_DINO = {"dino", "Wikidata", DINO_PROMPT_RULE, false, dinoFetch, dinoUnload, dinoBegin, dinoFrame, dinoEnd, dinoButtonB, dinoButtonC, nullptr, nullptr, dinoHints};`

- [ ] **Step 1: Write the failing test.** In `test_model.cpp`: `CHECK(parseModelRef("dino:Tyrannosaurus", r) && strcmp(r.kind, "dino") == 0);`. Run `tests/model/run.sh`. Expected: FAIL.

- [ ] **Step 2: Add `"dino"` to `MODEL_KINDS`.** Run `tests/model/run.sh`. Expected: PASS.

- [ ] **Step 3: Implement `feature_dino.ino`.**
  - **Fetch:**
    - One deadline, `millis() + EDU_LOOKUP_DEADLINE_MS`.
    - `dinoIndexName`, then GET `EDU_DATA_BASE_URL "dino/" + indexFile`, then `dinoResolve`, then GET `<slug>.txt`, then `dinoParseSheet`.
    - If the sheet has a picture, GET `<slug>.jpg` into PSRAM. If `!dinoImageOk`, set `hasImage = false`.
    - Then `dinoWorthShowing`, or return false.
    - `modelLabel` is the genus name, and `modelSourceName` is `"Wikidata"`.
  - **Begin:** read the language as `timelineReadLanguage` does. Decode the picture once into a 240×117 PSRAM sprite, then build the rows and the pages.
  - **Frame:** draw the current page as in the mock.
    - Picture page: the picture, a dark band with the name in citrus (`CITRUS`), and the group note.
    - Facts pages: the name in citrus, the note, then the label and value rows.
    - Size page: draw from `dinoSizeLayout`. The person and its label are white, the bar is `CITRUS`, and the range piece is darker. The title is the size-page heading from Global Constraints. The length text sits on the bar, or above the line.
    - Fun-fact page: the heading and the wrapped fun fact.
    - Footer: `eduDrawFooter`, with the credit on the picture page, `funSource` on the fun page and `source` otherwise. The page counter `2/4` sits on the right, as in `timelineDrawFooter`.
    - Pages change instantly, without an animation (my call).
  - **Buttons and hints:**
    - Buttons and tilt follow Global Constraints.
    - Hints are as in `timelineHints`: `HINT_EARLIER` and `HINT_LATER`, and `HINT_TEXT` on the last page.
  - **`DINO_PROMPT_RULE`**, exactly:

```cpp
static const char DINO_PROMPT_RULE[] =
    "DINOSAURIER: Setze 'model' auf 'dino:<Gattung>' wenn der Nutzer nach einem bestimmten Dinosaurier, Flugsaurier, "
    "Meeresreptil aus der Dinosaurierzeit oder Urvogel fragt (z.B. 'Erzaehl mir vom T-Rex' ergibt 'dino:Tyrannosaurus', "
    "'Wie gross war ein Brachiosaurus?' ergibt 'dino:Brachiosaurus'). Bei allgemeinen Fragen ('Warum starben die "
    "Dinosaurier aus?') bleibt model leer. Beantworte die Frage im Text wie gewohnt, nenne dort aber keine Laengen, "
    "Groessen, Gewichte, Geschwindigkeiten oder Jahreszahlen: die zeigt Zitro belegt in einem Steckbrief. "
    "Erwaehne den Steckbrief nicht selbst. Fuelle image_query auch dann wie immer.";
```

  - **Miss line (my call):** the spec's example sentence "siehst du gleich im Steckbrief" would be false whenever the lookup fails, so the AI no longer mentions the sheet. When `gptResponse.model` starts with `dino:` and `hasModel` is false, the main flow in `sketch_jan12a.ino`, next to the music miss, appends `dinoMissLine(english)` to the answer.
  - `const char* dinoMissLine(bool english)` lives in `dino_core.h` and returns `"Einen Steckbrief dazu habe ich nicht."` / `"I don't have a fact sheet for that."`. This task adds it to `dino_core.h`, and adds a check of both strings to `testFormats` in `tests/dino/test_dino.cpp`.

- [ ] **Step 4: Run the host tests.** Run `tests/dino/run.sh` and `tests/model/run.sh`. Expected: both pass.

- [ ] **Step 5: Compile the sketch.** Run `./verify.sh`. Expected: it compiles.

- [ ] **Step 6: Commit.** Message: `Show a fact sheet when someone asks about a dinosaur`.

### Task B6: Documentation

**Files:**
- Create: `docs/features/dino.md`, with the same sections as `docs/features/timeline.md`: Requirements, User guide, Release note, Sources and licences.
- Modify: `REQUIREMENTS.md`, `USERGUIDE.md`, `RELEASE_NOTES.md`

- [ ] **Step 1: Write `docs/features/dino.md`.** It covers:
  - the two tiers, the data rules, the file formats and the pages
  - the 170 cm person, and the minimum content
  - the written answer ("Beides", approved by the user on 2026-11-12)
  - the groups and their notes
  - in plain words, that no person reviewed the fun facts, the speeds or the automatic-tier pictures, and that the building AI chose them
  - the licence counts and `credits.html`, which must be uploaded at `https://zitro.spot/data/dino/credits.html`

- [ ] **Step 2: Update the user docs.**
  - `USERGUIDE.md`: one section: ask about a dinosaur by name; C, B and tilt to page; what the size comparison shows.
  - `RELEASE_NOTES.md`: two bullets under today's date, in simple, active language.
  - `REQUIREMENTS.md`: one line pointing to `docs/features/dino.md`.

- [ ] **Step 3: Commit.** Message: `Document dinosaur fact sheets`.

## Task C: Upload and test on the stick (needs the user's go-ahead)

- [ ] **Step 1: Ask the user** for the go-ahead to upload. Uploading is outward-facing.

- [ ] **Step 2: Upload.** Upload to `<user>@<cyon-host>:public_html/zitro.spot/`, the site root that `webpage2/` maps to. Do this before any firmware with this feature leaves the bench: a stick whose server has no sheets falls back to the picture and says it has no fact sheet. Upload in this order, so no index ever names a sheet that isn't there yet:
  1. Sheets, pictures and `credits.html`, without the indexes and **without `--delete`**: `rsync -av --exclude 'index-*' webpage2/data/dino/ <user>@<cyon-host>:public_html/zitro.spot/data/dino/`. (rsync keeps receiver files that match `--exclude` unless `--delete-excluded` is given, so `--delete` here would remove dropped sheets while the old indexes still list them.)
  2. Then the indexes, which is the whole folder again, and only now the stale files go: `rsync -av --delete-after webpage2/data/dino/ <user>@<cyon-host>:public_html/zitro.spot/data/dino/`. The new indexes are in place before anything is deleted.
  3. The timeline, pictures before the index: `webpage2/data/timeline/life_history.jpgs`, then `life_history.txt` and `credits.html`, to `.../zitro.spot/data/timeline/`, with `rsync -av` and no `--delete`.
  4. Last, `webpage2/index.html` to `.../zitro.spot/index.html`. Its Sources list links both credit pages (items 9 and 15).
  - `--delete-after` (step 2 only) is needed on re-uploads, because the generator empties `webpage2/data/dino/` and writes it anew, so a sheet or picture it dropped must go from the server too. Use it **only** on `data/dino/`, with the trailing slashes as above. On `data/` or the site root it would delete the other features' files.
  - Check with `curl -sI https://zitro.spot/data/dino/index-ty.txt` and `curl -sI https://zitro.spot/data/dino/credits.html`. Expected: 200 for both.
  - Measure the three HTTPS requests of one lookup (`index-ty.txt`, `tyrannosaurus.txt`, `tyrannosaurus.jpg`) against `EDU_LOOKUP_DEADLINE_MS` (10 s): from here with `curl -w '%{time_total}\n'`, and on the stick from the serial line `Dino '...': ..., in N ms` in Step 3.

- [ ] **Step 3: Coordinate the stick.** Check with the other sessions first (`ListAgents`, then `SendMessage`), then flash.
  - Over serial: `model dino:Tyrannosaurus`, `model dino:Pteranodon`, `model dino:<a rare genus without a picture>`, `model dino:Velociraptor mongoliensis`, `model timeline:life_history`.
  - Then one voice question per language.
  - Record the results and timings in `docs/features/dino.md` and `docs/features/timeline.md`. Say plainly what was not tested.

- [ ] **Step 4: Commit.** Commit the test record. Push or open a PR only when the user asks.
