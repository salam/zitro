# Song Library, Stage 1: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Zitro looks up songs it doesn't have built in on zitro.spot, plays them on the existing music band, and says so honestly when it doesn't know one. The first 20 children's songs and evergreens go on the server, each with two agreeing scores and documented copyright evidence.

**Architecture:**
- **Firmware:** the music feature first looks for a built-in song by normalised title, then fetches `data/music/index.txt` and `<id>.txt` from zitro.spot, like the protein feature. A failed lookup replaces the AI's answer with a fixed sentence.
- **Data:** an offline Python tool turns hand-kept song files in `scripts/music_library/` into the server files. It refuses any song without evidence, or whose two scores disagree.

**Tech Stack:** ESP32 Arduino (M5Unified), plain C++17 headers with host tests, Python 3 standard library.

**Spec:** `docs/superpowers/specs/2026-11-07-song-library-design.md`. Read it before any task.

**Scope:**
- **This plan:** rollout stage 1, plus the start of stage 2 (Task 9).
- **Later plans:**
  - Mutopia hymns and single-line pieces, starting with the spike that decides MIDI or LilyPond.
  - The hand-picked famous themes, which the user hears on the stick.

## Global Constraints

- **Legal rule (spec):**
  - Everyone who wrote the melody died before 1926, or the melody is traditional with a named print from before 1931.
  - First published before 1931.
  - Author and dates are confirmed by two independent reputable sources that agree.
  - For a known composer who died before 1926, the death is enough evidence of early publication. If a source mentions a late first edition, the song stays out.
- **Notes:**
  - Score A is the recorded source of the notes: a printed songbook from before 1931, or a public domain, CC0 or CC BY score.
  - Score B, compare only, is any openly licensed score, CC BY-SA included.
  - A and B must agree note by note, after allowing for a constant transposition. Any other difference is a rejection unless a `settled` line explains it.
  - All-rights-reserved sources are never used.
  - Our song data never copies notes from a CC BY-SA score.
- **No fuzzy matching anywhere.** Exact match on the normalised form only. A title that matches two songs matches none.
- **The AI only supplies a title.** It never supplies notes, and it never promises playback.
- **Built-in songs keep working without WiFi.** Old `music:<id>` references, for example after deep sleep, still resolve.
- **Firmware code:**
  - All `.ino` files compile as one translation unit, main first, then alphabetical. Statics need a feature prefix, and anything used before its definition needs an explicit prototype in `sketch_jan12a.ino`, next to `bool modelButtonCHold();`.
  - New struct fields go last, because initialisers are positional.
- **Prompt text** is ASCII only, written in German like the rest of the prompt.
- **Every `.ino` change:** `./verify.sh` must pass. If it fails with M5GFX linker errors, run `arduino-cli compile --clean` once with the same flags.
- **Every music change:** `tests/music/run.sh` must pass. `tests/protein/run.sh` must pass after Task 1.
- **Git:**
  - Work on branch `song-library-design` in the song-library worktree.
  - End commit messages with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
  - Never push `master`, `main` (local) or `feature/3d-view`. Push only this branch, by explicit refspec.
  - Never print or commit secrets. `local_secrets.h` stays untracked.
- **Docs:** update `docs/features/music.md`, `REQUIREMENTS.md`, `USERGUIDE.md` and `RELEASE_NOTES.md`. Release notes are simple, activating and aimed at end users, grouped under the date, with entries within 4 hours sharing one heading.

## Review Focus

1. **A German child asks in English for a German song**, such as "Play Brother John". Expected: built-in aliases in both languages find it. *Test in Task 3.*
2. **Accented or umlaut titles**, such as "Für Elise", "Frère Jacques", "Hänschen klein" or "Funiculì, Funiculà". Expected: they normalise to plain ASCII and match. *Test in Task 1.*
3. **The server answers 200 with an HTML error page, or a cut-off index.** Expected: no song, and the "can't reach my song list" sentence. Never a crash, never a partial match. *Test in Task 2.*
4. **The index matches, but the song file is missing, broken or for another id.** Expected: no song, the "don't know that song" sentence, and memory freed. *Test in Task 2.*
5. **No WiFi.** Expected: built-in songs play, and server titles get the "can't reach" sentence. *Stick test in Task 10.*

---

### Task 1: Shared name normalising

**Files:**
- Create: `sketch/sketch_jan12a/name_match.h`
- Modify: `sketch/sketch_jan12a/protein_core.h` (move `proteinNormalise`'s body out, keep the function as a wrapper)
- Test: `tests/music/test_music.cpp` (new `testNormaliseTitles`), `tests/protein/run.sh` must stay green

**Interfaces:**
- Produces: `inline bool eduNormaliseName(const char* in, char* out, size_t capacity, bool forTitles);` in `name_match.h`.
  - With `forTitles == false`, the result is byte for byte what `proteinNormalise` returns today.
  - With `forTitles == true`, it also folds UTF-8 Latin-1 accents to the base letter (à á â ã å → a, ç → c, è é ê ë → e, ì í î ï → i, ñ → n, ò ó ô õ → o, ù ú û → u, capitals too). Umlauts still spell out (ä → ae).
  - Apostrophes (`'`, and U+2019 `’`) are dropped with no space.
  - Any other ASCII character that isn't a letter, digit or space becomes a space. Spaces then collapse as today.
- `proteinNormalise(in, out, cap)` becomes `return eduNormaliseName(in, out, cap, false);`

- [ ] **Step 1: Write the failing test** `testNormaliseTitles()` in `test_music.cpp`, called from `main`:

```cpp
char out[128];
CHECK(eduNormaliseName("F\xC3\xBCr Elise", out, sizeof(out), true) && !strcmp(out, "fuer elise"));
CHECK(eduNormaliseName("Fr\xC3\xA8re Jacques", out, sizeof(out), true) && !strcmp(out, "frere jacques"));
CHECK(eduNormaliseName("Funicul\xC3\xAC, Funicul\xC3\xA0", out, sizeof(out), true) && !strcmp(out, "funiculi funicula"));
CHECK(eduNormaliseName("Oh! Susanna", out, sizeof(out), true) && !strcmp(out, "oh susanna"));
CHECK(eduNormaliseName("Beethoven's  Symphony", out, sizeof(out), true) && !strcmp(out, "beethovens symphony"));
CHECK(eduNormaliseName("Beethoven\xE2\x80\x99s Symphony", out, sizeof(out), true) && !strcmp(out, "beethovens symphony"));
CHECK(eduNormaliseName("  H\xC3\xA4nschen-klein ", out, sizeof(out), true) && !strcmp(out, "haenschen klein"));
CHECK(!eduNormaliseName("!!!", out, sizeof(out), true));          // Nothing left
CHECK(!eduNormaliseName("\xE6\x97\xA5\xE6\x9C\xAC", out, sizeof(out), true));  // Other scripts: no match
```

- [ ] **Step 2: Run `tests/music/run.sh`.** Expected: compile error, `eduNormaliseName` undeclared.
- [ ] **Step 3:** Create `name_match.h` with `eduNormaliseName`, include it in `music_core.h` and `protein_core.h`, and make `proteinNormalise` the wrapper.
- [ ] **Step 4: Run `tests/music/run.sh` and `tests/protein/run.sh`.** Expected: both report 0 failures.
- [ ] **Step 5:** Run `./verify.sh`. Expected: `✅ Compilation successful!`
- [ ] **Step 6: Commit:** "Share name normalising between proteins and songs"

### Task 2: Server index and song files in `music_core.h`

**Files:**
- Modify: `sketch/sketch_jan12a/music_core.h`
- Test: `tests/music/test_music.cpp` (`testSongIsSound`, `testResolve`, `testSongFile`, and `testServerData` over `webpage2/data/music` when that folder exists)
- Modify: `tests/music/run.sh` (pass `../../webpage2/data/music` as the first argument; the test skips `testServerData` with a message when the folder is missing)

**Interfaces:**
- Consumes: `eduNormaliseName(..., true)` (Task 1).
- Produces:
  - `MUSIC_MAX_NOTES` raised from 200 to 1000.
  - `#define MUSIC_INDEX_LIMIT (64 * 1024)` and `#define MUSIC_FILE_LIMIT (16 * 1024)`.
  - `bool musicSongIsSound(const MusicNote* notes, int count, int totalTicks, int barTicks, int pickupTicks, int beatsPerMinute)`. This moves today's checks out of `testEverySong` unchanged: more than 8 notes, more than 8 sounding, bpm 60 to 160, `barTicks` of 8, 12 or 16, pickup under one bar, `totalTicks % barTicks == 0`, MIDI 57 to 81, staff step -4 to 10, and no untied note across a bar line. `testEverySong` then calls it.
  - `struct MusicIndexEntry { char id[40]; char de[48]; char en[48]; char source[40]; };`
  - `bool musicResolve(const char* index, size_t indexLen, const char* title, MusicIndexEntry& out)`. The index must start with `ZITRO-MUSIC-INDEX 1 <count>`, followed by exactly `<count>` lines `id|de|en|aliases|source`. Ids use `[a-z0-9-]`; aliases are `;`-separated and already normalised. It matches the normalised title against `de`, `en` and each alias. False if anything in the index is malformed, or if the title matches two entries.
  - `bool musicParseSongFile(char* body, size_t length, const char* id, int& beatsPerMinute, int& barTicks, int& pickupTicks, const char*& notes)`. The file is exactly three lines: `ZITRO-MUSIC-SONG 1 <id>`, then `<bpm> <barTicks> <pickupTicks>`, then the notes in today's text format. It writes a terminator after the notes and returns false for a different id or any extra or missing line.

- [ ] **Step 1: Write the failing tests:**
  - `testResolve`:
    - A good two-entry index finds the first by `de`, `en` and an alias.
    - The same index with its count off by one finds nothing.
    - An HTML page (`"<!DOCTYPE html>..."`) finds nothing.
    - An index where two entries share an alias finds nothing for that alias, and still finds the others.
    - An id with a slash or a capital finds nothing.
  - `testSongFile`:
    - A good file returns its values, and `notes` parses with `musicParseNotes`.
    - A wrong id is refused, as are a missing third line, a fourth line, and a body of length 0.
  - `testSongIsSound`:
    - Every built-in song passes.
    - These are refused: a copy with one note moved to MIDI 90, one with bpm 200, and one whose last bar is a sixteenth short.
- [ ] **Step 2: Run `tests/music/run.sh`.** Expected: compile errors for the three new functions.
- [ ] **Step 3:** Implement them in `music_core.h`. For line splitting, reuse `sdfNextLine` from `model_core.h` (as `proteinResolve` does).
- [ ] **Step 4: Run `tests/music/run.sh`.** Expected: 0 failures.
- [ ] **Step 5: Commit:** "Read the server's song index and song files"

### Task 3: Built-in lookup by title, and the answer when a song isn't found

**Files:**
- Modify: `sketch/sketch_jan12a/music_songs.h` (add the `aliases` field last to `MusicSong`, `musicFindBuiltIn`, `musicSuggestion`, `musicMissAnswer`, and the new `MUSIC_PROMPT_RULE`)
- Test: `tests/music/test_music.cpp` (`testFindSong` rewritten, `testPromptRule` rewritten, new `testMissAnswer`)

**Interfaces:**
- Consumes: `eduNormaliseName` (Task 1).
- Produces:
  - `MusicSong` gains `const char* aliases;` as its last field. It holds `;`-separated, already normalised names and may be `nullptr`.
  - `const MusicSong* musicFindBuiltIn(const char* title)`. It replaces `musicFindSong`. Matches, in order:
    1. the exact id, case aside, so old `music:<id>` references keep working;
    2. otherwise, the normalised title equal to the normalised `nameDe`, `nameEn` or any alias.

    It returns `nullptr` if the title matches two songs.
  - `enum MusicLookup { MUSIC_LOOKUP_FOUND, MUSIC_LOOKUP_UNKNOWN, MUSIC_LOOKUP_OFFLINE };`
  - `const MusicSong* musicSuggestion(bool german, unsigned seed)`. It picks by `seed % 4` from these ids:
    - German: `alle-meine-entchen`, `frere-jacques`, `summ-summ-summ`, `backe-backe-kuchen`
    - English: `twinkle-twinkle`, `row-your-boat`, `london-bridge`, `mulberry-bush`
  - `void musicMissAnswer(MusicLookup reason, bool german, unsigned seed, char* out, size_t capacity)`. It writes the exact sentence, with `%s` filled by the suggestion's name in the device language:
    - DE, unknown: `Dieses Lied kenne ich nicht. Probier mal %s!`
    - DE, offline: `Ich erreiche gerade meine Liederliste nicht. Probier mal %s!`
    - EN, unknown: `I don't know that song. Try %s!`
    - EN, offline: `I can't reach my song list right now. Try %s!`
- Aliases to add, at least:
  - `ode-to-joy`: `beethovens sinfonie;beethovens symphony;beethoven symphony;beethovens neunte;beethovens ninth;freude schoener goetterfunken`
  - `frere-jacques`: `bruder jakob;brother john;are you sleeping`
  - `happy-birthday`: `zum geburtstag viel glueck`
  - `stille-nacht`: `silent night`
  - `twinkle-twinkle`: `twinkle twinkle little star`

  Add an English alias wherever a German song has a common English name, and the reverse.
- `MUSIC_PROMPT_RULE` (ASCII, German) must say each of these:
  - For any wish to hear, play, sing or see the notes of music, set `model` to `music:<title>`, using the best-known title in its original language and without the composer. The examples must include `'Spiel Alle meine Entchen' ergibt 'music:Alle meine Entchen'`, `'Play Silent Night' ergibt 'music:Silent Night'`, `'Spiel Beethovens Sinfonie' ergibt 'music:Ode an die Freude'`, `'Play Mozart' ergibt 'music:Mozart'` and `'Sing mir ein Lied' ergibt 'music:ein Lied'`.
  - Then `is_audio` is false, and `answer` is only the title, with no promise to play. The device checks whether it knows the song.
  - Questions about a song (`'Wer schrieb Stille Nacht?'`) leave `model` empty.
  - Never give notes.
  - `answer` in the language of the question.
  - Fill `image_query` as always.

  It no longer lists ids.

- [ ] **Step 1: Write the failing tests:**
  - `testFindSong`:
    - Every built-in is found by its id, `nameDe` and `nameEn`.
    - `musicFindBuiltIn("Play")` returns `nullptr`; so does `"alle meine"`.
    - `musicFindBuiltIn("Brother John")` returns `frere-jacques`, and `musicFindBuiltIn("BEETHOVEN'S SYMPHONY")` returns `ode-to-joy`.
    - No normalised name or alias is shared by two songs.
  - `testMissAnswer`:
    - All four sentences, with seed 0, are exactly as above with the first suggestion filled in.
    - Every suggestion's name is ASCII.
    - `capacity` 10 truncates safely.
  - `testPromptRule`:
    - The rule is ASCII.
    - It contains `is_audio false` and `music:<Titel>`.
    - Every `ergibt 'music:X'` resolves through `musicFindBuiltIn`, except `Mozart` and `ein Lied`, which must give `nullptr`.
    - It contains no song id with a hyphen.
- [ ] **Step 2: Run `tests/music/run.sh`.** Expected: failures and compile errors.
- [ ] **Step 3:** Implement. Rename every call of `musicFindSong` to `musicFindBuiltIn` (`feature_music.ino`, tests).
- [ ] **Step 4: Run `tests/music/run.sh`.** Expected: 0 failures, and `check_references` still compares 26 songs.
- [ ] **Step 5: Commit:** "Find built-in songs by title and word the not-found answer"

### Task 4: Fetching from zitro.spot, and the answer swap

**Files:**
- Modify: `sketch/sketch_jan12a/feature_music.ino` (`musicFetch`, new `musicMissAnswerForDevice`)
- Modify: `sketch/sketch_jan12a/sketch_jan12a.ino` (prototype; the swap after `fetchModel`; move `memoryAdd` below it)

**Interfaces:**
- Consumes: Tasks 2 and 3, plus `eduHttpGet`, `EDU_DATA_BASE_URL` and `EDU_LOOKUP_DEADLINE_MS` (as `feature_protein.ino` uses them).
- Produces:
  - `static MusicLookup musicLastLookup`, set on every `musicFetch`.
  - `String musicMissAnswerForDevice()`. It uses the stored language (`settings`, `language`, `"DE"` default, as `musicFetch` reads it today) and `millis()` as the seed.
- `musicFetch(query)` order:
  1. Try `musicFindBuiltIn(query)` first. Found means `MUSIC_LOOKUP_FOUND`, exactly as today.
  2. Otherwise, without WiFi (`WiFi.status() != WL_CONNECTED`), or if the index GET fails, the result is `MUSIC_LOOKUP_OFFLINE`.
  3. Otherwise, if the index has no match, or the song file fails `musicParseSongFile`, `musicParseNotes` or `musicSongIsSound`, the result is `MUSIC_LOOKUP_UNKNOWN`.
  4. On success: notes go in the existing `ps_malloc` buffer; `modelLabel` is `de` or `en` by device language; `modelSourceName` is the entry's `source`; the file URL is `EDU_DATA_BASE_URL "music/" + id + ".txt"`.
  5. Free every buffer on every path.
- In `handleVoiceQuery`, right after `fetchModel(modelRef);`: if `!hasModel` and `strcmp(modelRef.kind, "music") == 0`, then `gptResponse.answer = musicMissAnswerForDevice(); response = wordWrap(gptResponse.answer, 25);`. Move the existing `memoryAdd(question, gptResponse.answer)` block below this, so memory holds what the child saw.

- [ ] **Step 1:** Implement both changes.
- [ ] **Step 2: Run `tests/music/run.sh`.** Expected: 0 failures.
- [ ] **Step 3:** Run `./verify.sh`. Expected: `✅ Compilation successful!`
- [ ] **Step 4:** Flash, then use serial (`scratchpad/ser.py`).
  - **Already checked here:**
    - `model music:Alle meine Entchen` gives `found`, label `Alle meine Entchen`.
    - `model music:Brother John` gives `found`.
    - `model music:alle-meine-entchen` gives `found` (old reference).
  - **Not checkable until Task 6 deploys data:** `model music:Oh Susanna` must give `not available`, with `Music: lookup unknown` logged. Add that log line in `musicFetch` for each result.
- [ ] **Step 5: Commit:** "Look songs up on zitro.spot and say so when one is missing"

### Task 5: The build tool

**Files:**
- Create: `scripts/music_lily.py` (move `pitch_name`, `Reader`, `join` and `read_score` out of `tests/music/check_references.py`; `check_references.py` imports them)
- Create: `scripts/make_music_data.py`
- Create: `scripts/music_library/README.md` (the song file format below, for people adding songs)
- Test: `tests/music/test_make_music_data.py`, run from `tests/music/run.sh`

**Interfaces:**
- Consumes: `read_score(notes, lang, start) -> list[[midi, ticks]]` from `music_lily.py`.
- Produces:
  - `python3 scripts/make_music_data.py [--out webpage2/data/music]` reads every `scripts/music_library/*.song`.
  - It writes `index.txt`, one `<id>.txt` per song and `SOURCES.txt`. That is one block per song: names, score A citation and licence, score B URL, composers with dates and both source URLs, first print, and any `settled` note.
  - On any failure it writes nothing and exits 1, as `make_protein_data.py` does.
- Song file format (`<id>.song`, UTF-8, one `key value` per line, `#` comments):

```
id oh-susanna
de Oh! Susanna
en Oh! Susanna
aliases oh susanna
tempo 112
bar 8
pickup 2
notes <today's note format, as read from score A>
source-a Stephen Foster, Oh! Susanna, W. C. Peters, Cincinnati 1848 | <url of the scan> | pre-1931 print
score-b <url> | english | relative c' | <LilyPond notes>
composer Stephen Foster | 1826 | 1864 | <url 1> | <url 2>
published 1848 | <url>
footer Peters 1848
settled <optional: which difference a person accepted, and why>
```

- Checks, each with its own error message naming the song:
  - Required keys present.
  - `id` matches `[a-z0-9-]+` and differs from every built-in id.
  - No normalised name or alias equals another song's or a built-in's. The built-in names are parsed from `music_songs.h`, including its aliases.
  - `source-a` licence is one of `pre-1931 print`, `public domain`, `CC0`, `CC BY`.
  - **Every composer:**
    - died before 1926;
    - has two URLs on different hosts.
  - Or there are one or more `traditional <citation> | <year> | <url>` lines instead of `composer`. The year must be before 1931, and `published` is then optional.
  - `published` year before 1931 when present.
  - The notes parse the way the firmware will. Re-implement the C++ grammar in Python; `test_make_music_data.py` pins it to the same cases as `testParser`.
  - Bars add up: the same rules as `musicSongIsSound`.
  - Score B, read with `read_score`, equals score A after joining ties and allowing one constant transposition. If not, the song must carry a `settled` line, and the tool prints the difference anyway.
  - `index.txt` stays under 64 KB, and each song file under 16 KB.
  - The output passes `testServerData` (Task 2) when `tests/music/run.sh` runs.

- [ ] **Step 1: Write the failing tests** in `test_make_music_data.py`, using in-memory song texts, no downloads.
  - A good song passes.
  - Each of these fails with its own message:
    - composer died 1946;
    - published 1935;
    - only one composer URL;
    - two URLs on the same host;
    - `source-a` licence `CC BY-SA`;
    - alias equal to the built-in `bruder jakob`;
    - score B one note different without `settled`;
    - bar a sixteenth short;
    - a note at MIDI 90.
  - Score B transposed a fourth up passes.
- [ ] **Step 2: Run `tests/music/run.sh`.** Expected: import error for `make_music_data`.
- [ ] **Step 3:** Implement `music_lily.py`, `make_music_data.py` and the README, and switch `check_references.py` to the shared module.
- [ ] **Step 4: Run `tests/music/run.sh`.** Expected: 0 failures, and references still compare 26 songs.
- [ ] **Step 5: Commit:** "Add the tool that checks songs and writes the server's song files"

### Task 6: Five songs end to end (gate)

**Files:**
- Create: `scripts/music_library/<id>.song` × 5
- Create: `webpage2/data/music/` (generated)

The goal is to prove the route before scaling. Pick five songs that aren't built in, three German and two English, for example "Kling, Glöckchen", "Weißt du, wie viel Sternlein stehen", "Der Mond ist aufgegangen", "Oh! Susanna" and "Auld Lang Syne". Check every candidate against the legal rule first, and drop it if anything is unclear.

- [ ] **Step 1:** For each song, find score A: a scan of a printed songbook from before 1931 on IMSLP, the Internet Archive, Wikimedia Commons or Project Gutenberg. Record the exact page URL.
- [ ] **Step 2:** Read the melody from the scan, as an AI reading the image or by hand, into today's note format. Record nothing from score B while doing this.
- [ ] **Step 3:** Find score B: a different, openly licensed score, usually the `<score>` on de or en Wikipedia. Copy its LilyPond notes in, as `references.txt` does.
- [ ] **Step 4:** Collect the composer and first-print evidence: two sources that agree.
- [ ] **Step 5:** Run `python3 scripts/make_music_data.py`, then `tests/music/run.sh`. Expected: 5 songs written, 0 failures.
- [ ] **Step 6:** Copy `webpage2/data/music/` to the server:

```bash
rsync -av webpage2/data/music/ <deploy host>:<zitro.spot web root>/data/music/
curl -s https://zitro.spot/data/music/index.txt | head -1
```

Expected: `ZITRO-MUSIC-INDEX 1 5`.
- [ ] **Step 7:** On the stick, run `model music:<title>` for each of the five, then `model music:Oh Susanna` (or another of them) with the German and with the English name. Expected: `found`, and the band plays. Take one screenshot (`scripts/capture_screens.py ... shot`) and check that the footer shows the source.
- [ ] **Step 8: Gate.**
  - **Stop and report to the user if either is true:**
    - fewer than four of the five songs could be completed;
    - scans or score B sources turn out unusable.
  - **Otherwise:** commit the five songs and the data, "Put the first five songs on zitro.spot".

### Task 7: Audit of the 26 built-in songs

**Files:**
- Modify: `docs/features/music.md` (licence table)

- [ ] **Step 1:** For each built-in song, apply legal rules 1 to 3 (see the spec, "The legal rule") and the evidence rule. Composer and dates come from two sources, and a traditional melody needs a print from before 1931. Add the composer, their dates and both source URLs to the table.
- [ ] **Step 2:** List each song that fails or is unclear, with the reason. Check "Zum Geburtstag viel Glück" (`happy-birthday`) by name: Mildred Hill, melody, died 1916; Patty Hill, lyrics, died 1946. Lyrics aren't stored.
- [ ] **Step 3:** Remove nothing. Commit "Record copyright evidence for the built-in songs", then report the list of failing or unclear songs to the user.

### Task 8: Docs

**Files:** `docs/features/music.md`, `REQUIREMENTS.md`, `USERGUIDE.md`, `RELEASE_NOTES.md`

- [ ] **Step 1:** In `music.md`, rewrite the rules the spec lists under "Rules this changes":
  - title instead of id;
  - no id list in the prompt;
  - two scores for server songs;
  - `source` now shown;
  - memory up to 8 KB;
  - the firmware picks the suggested song.

  Add the lookup order, both fixed sentences, and the offline behaviour.
- [ ] **Step 2:** In `USERGUIDE.md`:
  - say that Zitro now finds many more songs when connected;
  - say what it answers when it doesn't know one;
  - say that built-in songs work without WiFi.

  Point to `SOURCES.txt` for where songs come from.
- [ ] **Step 3:** Add to `RELEASE_NOTES.md`, for example: "**More songs**: Zitro now finds songs on its own server, from Oh! Susanna to Kling Glöckchen. Each one is checked against two old songbooks or scores."
- [ ] **Step 4:** In `REQUIREMENTS.md`, point the music line to the new rules, and add the data folder to the server data list if one exists.
- [ ] **Step 5: Commit:** "Document the song library"

### Task 9: Fifteen more songs

**Files:** `scripts/music_library/*.song`, `webpage2/data/music/`

- [ ] **Step 1:** Add 15 more songs the same way as Task 6, mostly children's songs and a few evergreens. Candidates, each still to be checked:
  - Ihr Kinderlein kommet, Muss i denn, Die Vogelhochzeit, Laterne Laterne, Hoch soll er leben
  - Mary Had a Little Lamb, Three Blind Mice, Pop Goes the Weasel, Yankee Doodle, My Bonnie, Camptown Races
  - Greensleeves, Danny Boy (Londonderry Air), Santa Lucia, Funiculì Funiculà

  Leave out any whose evidence is thin, such as "Leise rieselt der Schnee": Eduard Ebel died in 1929.
- [ ] **Step 2:** Run the tool and the tests, deploy as in Task 6, step 6, and test 5 of them on the stick.
- [ ] **Step 3: Commit:** "Add fifteen more songs"

### Task 10: Stick trials, PR

- [ ] **Step 1:** Run the voice path with memory cleared. Use `scratchpad/musictrial.py` with a new question list, 3 runs each:
  - "Spiel Alle meine Entchen", "Play Brother John", "Spiel Kling Glöckchen", "Play Oh Susanna"
  - "Play Beethoven's symphony", "Play Mozart", "Sing mir ein Lied", "Play Bohemian Rhapsody"
  - "Wer schrieb Stille Nacht?"

  Expected:
  - Server songs and built-ins give `model music:...` and open.
  - Unknown ones show the exact fixed sentence. No answer says it is playing while nothing plays.
  - "Wer schrieb Stille Nacht?" has an empty `model`.

  Record the counts in `music.md`, as was done for the Beethoven fix.
- [ ] **Step 2:** With WiFi off (`ask` won't work, so use the serial `model` command):
  - `model music:Alle meine Entchen` must give `found`.
  - `model music:Oh Susanna` must give `not available`, and log `Music: lookup offline`.
- [ ] **Step 3:** Run `./verify.sh`, `tests/music/run.sh` and `tests/protein/run.sh`. Scan the branch for secrets with `python3 -I scratchpad/scanpub.py origin/main..song-library-design`; expected: only the known `sk-ant-` placeholder.
- [ ] **Step 4:** Push the branch by refspec and open a PR to `main`. In the PR, list what was checked by hand and what wasn't. Merge after the user says so.
