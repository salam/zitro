# Song library on zitro.spot: design

Status: draft for approval, 2026-11-07. Decisions marked **(user)** came from Matthias. Everything else is the author's call and can be overruled.

## Why

Kids ask Zitro to play songs it doesn't know. Today it knows 26 melodies built into the firmware. It should know many more, loaded from the Zitro server, without ever playing a wrong melody or publishing a song that is still protected somewhere.

The user chose confidence over size: **"Ok, wenn es Lieder nicht findet. Hohe confidence ist besser."** (user) A missing song is fine. A wrong note or a legal doubt is not.

## Goals

- Many more songs, in this order of importance: children's songs, evergreens, then classical pieces, which are asked for less often. (user)
- Sources: Mutopia, plus children's songs and evergreens that are safely unprotected worldwide. (user)
- Same music band, buttons and volume levels as today. Songs from the server look and behave exactly like built-in ones.
- The 26 built-in songs stay in the firmware and keep working without WiFi.

## Non-goals

- Harmony or several voices: the buzzer plays one note at a time.
- Lyrics, recordings, singing.
- Modern songs, film music, pop.
- "Play me any song" or "Play Mozart" picking a piece for the child. Those still get the honest answer. This is a separate decision, not part of this work.
- Querying Mutopia or any outside service from the stick. The stick only talks to zitro.spot.
- A size target. The number of songs is whatever passes the checks. A guess is low hundreds at first.

## Anti-goals: this has failed if

- A song plays with wrong notes, or the wrong song plays under the name asked for.
- A near match plays something the child didn't ask for.
- Anything on zitro.spot is still protected in any country we can reasonably check.
- The AI says it is playing a song and nothing plays. That was the bug fixed in PR #7, and this design could bring it back (see "When a song isn't found").
- The AI supplies a single note. It only ever supplies a title.

## What we learned about Mutopia

From the header fields of a shallow clone of `MutopiaProject/MutopiaProject` (last pushed 2024-11, 2,187 `.ly` files with Mutopia headers; one piece can span several files):

| Style | Files | | Instrument | Files |
|---|---|---|---|---|
| Baroque | 716 | | Piano | 498 |
| Classical | 579 | | Guitar | 251 |
| Romantic | 452 | | Organ | 177 |
| Folk | 110 | | Voice (SATB) | 132 |
| Hymn | 107 | | Voice and Piano | 74 |
| Song | 23 | | Voice | 38 |

- No title contains Twinkle, Oh Susanna, Scarborough, Swing Low, Saints or Lullaby. **Mutopia will not supply the children's songs.** It supplies classical pieces, hymns and some folk tunes. The children's songs and evergreens need their own route.
- Licences: newer files name one in a `license` field: Public Domain 245, CC BY-SA about 270, CC BY 57. About 1,600 older files have no such field. A rough text search suggests about 917 public domain and 694 Creative Commons, but this needs a proper reading per piece, and the `cc-rdfs` folder should be checked first.
- Mutopia's own rule is that composer, lyricist, arranger and editor have been dead for 70 years. That is not strict enough for our rule below, so its licence alone doesn't qualify a piece.
- The git repo has `.ly` files but no MIDI. Mutopia's download archive offers MIDI, and LilyPond 2.26 is available from Homebrew. It is not installed here.

## The legal rule

A song goes on the server only if **all** of these hold. **"Sicher global ungeschützt"** (user) is read as:

1. Everyone who wrote the melody died before 1926, or the melody is traditional with no known author. The longest term in common use is the author's life plus 100 years (Mexico).
2. The melody was first published before 1931, or the same traditional melody is documented in print before then. That covers the US rule as of 2026.
3. Lyrics don't count because we never store or show them. The name of a song is not protected.
4. The notes come from a source we may use: a printed songbook from before 1931, or a public domain, CC0 or CC BY score, the latter credited. CC BY-SA scores, such as Wikipedia's, may be used **only to compare**, as `tests/music/references.txt` does today. Their notes are never copied into our data. All-rights-reserved sources aren't used at all, not even to compare, because comparison means storing their notes in this public repo.

Every song records the evidence: who wrote it, their dates, where it was first printed, and the source of the notes. If the evidence is missing or disputed, the song stays out.

How much evidence is enough **(user, 2026-11-07)**:

- Author and dates come from two independent reputable sources, such as Wikipedia, IMSLP or the Liederlexikon. They must agree.
- A traditional melody also needs a named print from before 1931.
- For a well-known composer who died before 1926, the death alone is enough evidence of early publication. The remaining risk is a work first published long after its composer's death. Where a source mentions a late first edition, the song stays out.
- A person doesn't have to check each song. The tool fails if a field is empty or the two sources disagree. For example, the authorship of "O sole mio" is disputed, so it stays out.

This is a careful reading, not legal advice.

**Run rules 1 to 3 over the 26 built-in songs and report the result.** They keep their existing check against one published score and aren't subject to rule 4 or the two-score rule. Their melodies are public domain and were typed in, not copied. Where a second score turns up anyway, report any difference. Don't remove any song without asking. "Zum Geburtstag viel Glück" needs a look: the melody's author died in 1916, the lyricist in 1946.

## How notes get in

The confidence rule decides by kind of source:

**Children's songs and evergreens: two independent scores must agree, note by note.**

- Score A is the recorded source of the notes: a scan of a printed songbook from before 1931, or a public domain, CC0 or CC BY score.
- Score B is only compared and can be any openly licensed score, including CC BY-SA ones such as Wikipedia's.
- Agreement after allowing for transposition and repeat signs is a pass. A note followed by a rest counts as equal to a longer note of the same pitch when the next note starts at the same time (user decision, 2026-11-07). Any other difference is a rejection, unless a person settles it with a note in the data saying why.
- A scan may be read by software or an AI. That reading alone never counts. The check is that it matches an independent score, because a misread note is very unlikely to match by chance.
- A CC BY credit is owed only when a CC BY score is score A.
- Start with five songs end to end. If scans of old songbooks turn out too hard to use, come back to the user before going on; the 20 songs in phase 1 depend on this.

**Mutopia single-line pieces (voice, flute, violin, recorder and similar): one score, machine-read.** Mutopia names the edition it set from. The piece must be strictly one note at a time.

**Mutopia hymns (SATB): one score, the top voice only**, read the same way. Hymns are named by tune, so the index uses only names the source gives.

**Mutopia piano, guitar and orchestral pieces: only from a short hand-made list of famous themes**, such as Für Elise. A program can pick the wrong voice here. Each one needs its melody chosen by a person and heard once on the stick before it goes up. Expect dozens at most.

A song is rejected, never adjusted, if:

- notes overlap or come as chords on the melody line,
- any length doesn't fit the band's sixteenth-note grid (triplets, thirty-seconds). **Rounding would create wrong notes, so these stay out.**
- the bars don't add up,
- notes leave what the band can draw (`music_draw.h`) or the song uses a bass clef,
- the title isn't unique.

Tempo stays our choice, as it is today, taken from the score's marking if it has one.

## Data on the server

Same pattern as proteins and timelines: static files in `webpage2/data/music/`, deployed with the homepage.

- `index.txt`: header `ZITRO-MUSIC-INDEX 1 <count>`, then one line per song: `<id>|<name de>|<name en>|<aliases>|<source name>`. `<id>.txt`: tempo, bar length, lead-in and the notes in the existing text format from `music_core.h`.
- The index limit in `protein_core.h` is 16 KB. Music will need its own and larger one. The index is read into PSRAM only for the lookup and freed afterwards, as with proteins.
- `MUSIC_MAX_NOTES` is 200 today and must rise. A note is 8 bytes, so 1,000 is 8 KB of PSRAM. Long pieces simply loop like any other.
- A generated page lists every song with its source, licence and evidence. The CC BY licence needs that credit to exist. Where to link it is still open from the timeline work (`credits.html`).
- The tool that builds this data is `scripts/make_music_data.py`, like the other `make_*_data.py` scripts, with its song list and evidence in the repo.

## Finding a song

- The AI sets `model` to `music:<title>`, with the best-known title of what was asked for. It no longer needs the list of ids, so the prompt gets shorter.
- The stick first looks at built-in songs, then the server index. Built-in songs therefore keep working offline.
- Matching is exact on a normalised form: lower case, umlauts spelled out, punctuation dropped. **No fuzzy matching.** `musicFindSong` already refuses near matches for this reason.
- Names and aliases are written by hand in German and English. A title that could mean several pieces, such as "Wiegenlied", gets no alias. It is simply not found.
- "Beethoven's symphony" keeps opening the Ode to Joy, as an alias.

## When a song isn't found

This is the trap. The AI writes its answer before the stick knows whether the song exists. So it could say "Here is X" and find nothing.

- The prompt rule tells the AI to write only the title as `answer` when it sets `music:`, with no promise.
- All requests to hear music use `music:<title>`, including "Play Mozart". The AI no longer decides whether Zitro knows a song.
- If the lookup fails, the firmware **replaces the AI's answer** with a fixed sentence in the device language that also suggests a built-in song, as the honest answer does today. For example: "Dieses Lied kenne ich nicht. Probier mal Bruder Jakob!" The firmware picks the song, so the prompt needs no list. The screen and any spoken answer use that sentence.
- If the server can't be reached and the song isn't built in, a different fixed sentence says so and suggests a built-in song too. "I don't know it" would be untrue there.
- The picture from `image_query` still shows, as it does today.
- Nothing is cached on the stick between songs. Waking from deep sleep looks the song up again, like proteins, and needs WiFi for server songs.

## Rollout

1. Firmware and tool, with about 20 children's songs passing the two-score check. Test on the stick.
2. More children's songs and evergreens.
3. Mutopia hymns and single-line pieces.
4. The hand-made list of famous themes, each heard on the stick.

## Checks

- Host tests: index parsing, normalising, no duplicate names or aliases, and every file in `webpage2/data/music/` read by the real C++ parser and passing the same bar checks as `testEverySong`.
- The tool fails the build if a song lacks evidence, or if the two-score comparison fails.
- On the stick, with memory cleared (serial `forget`), each phrasing at least 3 times: a built-in song, a server song in German and English, an unknown song, a server song with no WiFi. Prompt results vary, so a single pass proves nothing.
- `./verify.sh` and `tests/music/run.sh`.

## Rules this changes

These need updating in `docs/features/music.md`, `REQUIREMENTS.md` and `USERGUIDE.md`:

- "Name a song from the list as an example" stays, but the firmware now picks the song, not the AI.
- "The id must exactly match the table": it becomes a title matched against table and index.
- "The prompt rule lists the ids": it no longer does.
- "Each is compared note by note with a published score": it stays for children's songs and evergreens, with two scores. **Mutopia pieces are read from one score by program instead.** That is a real loosening, accepted because Mutopia's source is a named edition.
- "`source` is empty": server songs name their source.
- "Memory: notes take 1.6 KB": more, up to 8 KB.

## Left to the builder

- Reading Mutopia: its MIDI files or LilyPond. Whichever gives exact notes and repeats reliably, proved on a sample against the score.
- How repeats are treated, provided the band's loop stays in time.
- Exact file syntax, limits and timeouts.
- Which scores count as independent, within the rule above. Two copies of one edition don't.
