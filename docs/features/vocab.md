# Vocabulary trainer

Design: `docs/superpowers/specs/2026-11-13-vocab-trainer-design.md`. Plan: `docs/superpowers/plans/2026-11-13-vocab-trainer.md`.
Logic in `sketch/sketch_jan12a/vocab_core.h` (tests: `tests/vocab/run.sh`), the mode in `vocab.ino`, the server in
`webpage2/api/voci.php` + `voci_lib.php` (tests: `tests/voci-server/run.sh`), the upload page in `webpage2/voci/`.

## Requirements

### Lists

- Zitro ships no word lists. Parents upload their own files at zitro.spot/voci. Lists belong to one stick, through a
  code the stick makes: 16 random bytes (`esp_random`), base32, 26 characters `A-Z2-7`, Preferences `voci/code`.
  No account, no name, no e-mail.
- The stick shows the code as a QR code for `https://zitro.spot/voci/#<code>`. The code sits in the URL fragment and
  travels to the server only in the header `X-Voci-Code`, never in a URL. The server stores lists under
  `sha256(code)`, so a directory listing doesn't reveal codes.
- Formats: CSV, TSV and semicolon files (UTF-8, UTF-16 with BOM, or Windows-1252 from Excel), Quizlet's text export,
  Anki's text export and Anki packages (`.apkg`, `.colpkg`) in the older format. Anki's newer, zstd-compressed format
  can't be read on the server: the page asks to export again with "Support older Anki versions" ticked.
- The server reads the file, the page shows a preview (count, first rows, warnings, book and name taken from the file
  name, columns swappable), and only then saves it.
- Limits: 5 MB per upload, 500 entries per list, 120 characters per side, 100 lists and 1 MB per code. New codes: at
  most 20 per IP per day; file previews: at most 120 per IP per hour (the IP is kept only as a salted hash).
  Anki packages are read only from an ordinary `notes` table (no view, virtual table or generated column), at most
  20000 notes. Lists that no stick fetched and nobody changed for a
  year are deleted. Parents can delete everything at any time.
- Lists are never shared between families. Public sources: none found that Zitro may offer (Quizlet has no open API,
  AnkiWeb decks state no licence, publishers sell their own packages). Parents can upload exports of their own sets.

### Starting

- New answer field `vocab`: `upload`, or `practice|<book>|<unit>`, `spell|<book>|<unit>`, `print|<book>|<unit>` with
  book and unit as the child said them, possibly empty. The AI never supplies words. `vocab` wins over everything
  but the tour.
- The stick finds the list itself (`vocabFind`): names folded like recipe names, number words become digits, a unit
  matches by name or by its number after "Unit", "Lektion", "Lesson", "Kapitel", "Chapter". No book: the current
  book. Several books with that unit and no current book: "Welches Buch?". No unit and no book: the list practised
  last, else "Welche Liste?" with the current book's lists. Nothing found or no lists: the QR screen.
- `upload` shows the QR screen. Any button leaves it.

### The trainer

- A takeover mode like the guided tour: `loop()` hands over to `vocabTick()` while `vocabActive()`. Button A answers,
  holding B doesn't reset WiFi, three presses of C don't open the settings. Button tabs stay on the screen.
- Start screen: book, unit, number of words and direction ("auf Englisch"), "Drück M5 zum Starten".
- Card (user, 2026-11-13): only a header and the word. Header: the C tab "?" then the unit name and one dot per card
  (citrus done, white now). Middle: the word, 14 pt, else 10 pt, else two lines. A tab: microphone; B tab: menu.
  Turned the other way round, the header moves to the bottom.
- Round: 10 cards. First the ones answered wrong last time, then the next ones from a cursor that walks the list.
  A wrong card comes back once at the end of the round. The score counts first attempts: "8 von 10".
  Stored per list in Preferences `voci` (`w<id>` wrong cards, `c<id>` cursor, `last`, up to 50 lists in `ids`).
- C on a card: "weiss nicht", shows and speaks the answer, counts as wrong.
- Menu (B): Weiter, Auf Deutsch / Englisch antworten, Buchstabieren / Wörter sagen, Voci-Blatt drucken, Beenden.
  Three rows visible with a scroll bar. Changing direction or mode starts a new round.

### Checking an answer

- The answer and every form of the list side are folded: lower case, apostrophes and quotes dropped, text in brackets
  dropped, hyphens and spaces ignored, a leading "to " dropped, British and American spellings and numbers made one
  (colour/color, maths/math, 2/two). Forms: alternatives split at "," ";" and " / ", "left/right" inside a phrase gives
  one phrase per word, "Schüler/-in" gives "Schüler" and "Schülerin", "(Schul-)Klasse" also "Schulklasse".
- Right: equal to a form, also with an article added or left out; for "How about …?" any words in the gap.
- "Fast!" (CLOSE): one or two words that sound like the answer but are written differently (gym/Jim, tie/Thai,
  ship/sheep). Shown with the answer, not counted right, not stored as wrong.
- Sentences (with "…", ending in . ? !, or three or more words) that aren't right by these rules go to the AI as
  `{"same": true|false}`. The answer shown is always the list's. No AI answer: "Ich bin nicht sicher", not stored.
- Wrong: "Ich habe gehört: «...»", "Richtig ist:" and the answer in citrus, spoken aloud.
- Right: a green check, the answer large, the prompt small, spoken aloud in British English.

### Spelling

- Only entries that are one word of letters (hyphens and apostrophes become fixed boxes). The card shows one empty box
  per letter.
- Recording: 16 kHz, `language=en`, prompt "A child spells an English word letter by letter, for example: Q, R, S, T."
  (no real word: one in the prompt pulled a transcript towards it). Up to 15 s, ends 1.2 s after the speech stops,
  never before 1.5 s. The noise level is measured in the first 0.3 s.
- Letters from the transcript: single letters and English letter names ("see", "aitch", "double you", "zed"),
  "double p" = "pp". A transcript with no letters, only a word, asks "Bitte Buchstabe für Buchstabe".
- The boxes fill one after another with what the child spelled, then the word is shown marked: green right, red
  different, orange missing, red boxes for extra letters. Sound-alike letters count as right: b/p, p/t, d/t, b/d, g/j,
  m/n, so a hearing error never marks the child down.

### The printed sheet

- From the menu or `print|...`: the printer list, then printing straight away (always A4, no size list).
- Pages of 18 rows: title "Young World 3 · Unit 1", Name and Datum lines, German left, a writing line right. Then the
  same pages with the answers ("Lösungen"). Text twice the size of the picture pages' text. One print job per page;
  the first page finds a resolution the printer accepts, the others use it. Footnote "Wörterliste: eigene Datei ·
  Gedruckt mit [lemon] https://zitro.spot".

### Sleep

- A press keeps the stick awake for 5 minutes. A press on a dark screen only wakes and redraws. Deep sleep ends
  the trainer.

### Serial commands

`vocab <value>` (as the answer field), `vocabkey a|b|c`, `vocabtype <text>` (stands in for the next recording),
`vocabcode`, `vocabnewcode`, `vocabhost <url>` (a test server, `http://` allowed).

## User guide

**For parents.** Say "Wörterlisten hochladen" to Zitro. Scan the QR code with your phone. On the page that opens,
upload the word list of your child's unit: a CSV or Excel file saved as CSV, a Quizlet text export or an Anki deck.
Check the preview, correct the book and name if needed, and save. Mark the book your child uses now as "aktuell".
The lists stay with your stick's code only. Nobody else sees them.

**For children.** Press M5 and say "Unit 3 Englisch üben". Zitro shows a German word. Press M5 and say it in English.
Zitro tells you if it's right and says the word for you. Don't know? Press the upper button (?). After 10 words you
see how many you knew. Words you didn't know come back first next time.

**Spelling.** Say "Unit 3 buchstabieren", or pick "Buchstabieren" in the menu (lower button). Spell the English word
with English letter names, one letter after the other: "tee, eye, em, ee...". Don't say "double", say every letter.
The boxes show what Zitro heard and which letter is missing or different.

**On paper.** Pick "Voci-Blatt drucken" in the menu, or say "Druck mir die Voci von Unit 3". You get pages to fill in
and pages with the answers.

## Release note

- **Practise your English words with Zitro**: upload your child's word list on your phone, then say "Unit 3 Englisch
  üben". Zitro asks, listens, and says each word for you.
- **Spell it**: say "buchstabieren" and watch the letter boxes fill. Zitro shows which letter is missing.
- **On paper too**: print a word sheet with an answer page.

## Spike: spelling and speech recognition (2026-11-13)

macOS voices, Young World 1 words plus deliberately misspelled ones, 1014 transcriptions with `gpt-transcribe`.

| Request | Word exactly right (English letter names) | German letter names | Single letter |
|---|---|---|---|
| As the stick sent it (8 kHz, nothing else) | 58 % | 28 % | 55 % |
| 16 kHz + `language=en` + spelling prompt | 88 % | 42 % | 91 % |

- Misspellings stayed misspelled (1 of 288 changed, by a real word in the prompt).
- German letter names don't work; "double p" is heard as W; confusions p/t, b/p, r/a, g/j.
- "grandmother" with natural pauses: up to 12 s.
- Computer voices, clean audio, not the stick's microphone: children at the stick will do worse.

## Sources and licences

Nothing is shipped. The lists are the families' own files. Textbook lists such as Young World (Klett und Balmer,
"Kopiervorlage") may be kept as a private copy for a family's own use; Zitro never publishes or shares them, and no
publisher content is in this repository (the tests use their own words).
