# Song library

Each `<id>.song` file here is one song that Zitro can fetch from zitro.spot. The tool `scripts/make_music_data.py` checks every song and writes `webpage2/data/music/`. That is `index.txt`, one `<id>.txt` per song, `SOURCES.txt` with each song's evidence, and an `.htaccess` that makes the server send them as UTF-8, so the umlauts in `SOURCES.txt` show correctly in a browser.

```bash
python3 -I scripts/make_music_data.py    # --out DIR to write elsewhere, --library DIR to read elsewhere
tests/music/run.sh                       # The firmware's own code then reads what was written,
                                         # and the folder must be exactly what the tool builds (--check)
```

If any check fails, the tool names the song and the reason, writes nothing and exits 1. A song is never adjusted to pass. It is fixed by hand or left out.

## The format

The file is UTF-8, with one `key value` per line. Every key needs a value, and only a newline ends a line: tabs and other control characters are refused. Lines starting with `#` are comments, and blank lines are ignored. Fields within a line are separated by `|`.

```
id oh-susanna
de Oh! Susanna
en Oh! Susanna
aliases oh susanna
tempo 112
bar 8
pickup 2
notes <the notes, as read from score A>
source-a Stephen Foster, Oh! Susanna, W. C. Peters, Cincinnati 1848 | <url of the scan> | pre-1931 print
score-b <url> | english | relative c' | <LilyPond notes>
composer Stephen Foster | 1826 | 1864 | <url 1> | <url 2>
published 1848 | <url>
footer Peters 1848
settled <optional: which difference a person accepted, and why>
```

### Names and timing

| Key | What it holds |
|---|---|
| `id` | Lower-case letters, digits and `-`, at most 39 characters. The file must be named `<id>.song`. It must not be a built-in song's id, nor `index` or `sources`. |
| `de`, `en` | The German and the English title, both required, because the AI may give either. Each is at most 47 bytes, with no `\|`. Both must normalise: accents and umlauts are fine, other scripts are not. |
| `aliases` | Optional. Other names, `;`-separated, already in normalised form: lower case, umlauts spelled out (`ae`, `oe`, `ue`, `ss`), accents dropped, apostrophes dropped, other punctuation as single spaces. For example, `kling gloeckchen;kling gloeckchen klingelingeling`. A title that could mean several pieces, such as "Wiegenlied", gets no alias. |
| `tempo` | Quarter notes a minute, 60 to 160. |
| `bar` | The length of a bar in sixteenths: 8 (2/4), 12 (3/4) or 16 (4/4). |
| `pickup` | The lead-in before the first bar line, in sixteenths, from 0 to one less than a bar. The last bar makes up for it. |
| `notes` | The melody, on one line, in the stick's format, for example `D4:2 \| G4:2 G4:2 B4:4~ B4:2`. A note is a letter `C D E F G A B`, then optionally `#` or `b`, then an octave from 2 to 6. A rest is `R`. Each is followed by `:` and a length in sixteenths: 1, 2, 3, 4, 6, 8, 12 or 16. `~` ties a note to the next, which must be the same pitch. `\|` marks a bar line for the reader only. Notes must lie between A3 and A5, and no note may run over a bar line unless it is tied. |
| `footer` | The source's short name, which the stick shows under the notes. At most 39 bytes. |

No name or alias may equal, once normalised, a name or alias of another song or of a built-in song. The stick looks at built-in songs first, and a title two songs share finds neither.

### Where the notes come from

| Key | What it holds |
|---|---|
| `source-a` | Score A, the source of the notes: `citation \| url \| licence`. The licence is one of `pre-1931 print`, `public domain`, `CC0`, `CC BY`. A CC BY score is credited by its citation in `SOURCES.txt`. |
| `score-b` | Score B, an independent score that is only compared and never copied: `url \| language \| octaves \| notes`. It must be openly licensed, and may be CC BY-SA, such as Wikipedia's. An all-rights-reserved score may not be used, not even to compare, because its notes would be stored in this public repo. The tool can't see the licence, so check it by hand. The language is the LilyPond note language: `nederlands` (LilyPond's default, `fis` and `bes`), `deutsch` (`h` is B and `b` is B flat), or `english` (`fs` and `bf`). The octaves are `relative <pitch>` as in `\relative c'`, or `absolute`. The notes are LilyPond notes and rests only, as in `tests/music/references.txt`: lengths, dots, ties, `\|`, `\repeat unfold` and `\repeat volta` with `\alternative`. Leave out marks, chords, lyrics, `\key`, `\time`, `\partial` and other voices. Write a repeat without alternative endings as `\repeat unfold 2 { … }`: `\repeat volta` needs an `\alternative`. |
| `settled` | Only when score A and B differ: which difference a person accepted, and why. |

Score B, with ties joined and repeats written out, must equal score A note by note, with the same lengths. Only a constant transposition is allowed. One more difference is allowed: a note followed by a rest counts as equal to one longer note of the same pitch, as long as the next note starts at the same time (so `G4:8 R:4` equals `G4:12`, but `G4:8 R:4 A4:4` does not equal `G4:8 A4:8`). At the end of the song the same holds with the song's end as the next note: the last note and the rests after it count as one length, so both scores must still be equally long overall (`G4:4 R:4` equals `G4:8` there, but not `G4:4`). This holds in the comparison only; the served notes are never changed. If they differ, the tool refuses the song and names every place where they differ, not just the first. It finds the transposition from the whole song, the move under which the most notes agree, so a wrong first note or a repeat only one score has doesn't hide the rest. If B is written at doubled or halved values, the places are listed at A's lengths, but that is still a difference. With a `settled` line the tool accepts the song, still prints every difference, and lists them under the settled line in `SOURCES.txt`. A `settled` line on two scores that agree is refused too, so an old note can't hide a new difference.

### Licences of the score B notes

The melodies are public domain. The score B notes stored in these files are not all ours, so each `.song` file says in its first line where its score B notes come from and under which licence:

- **Wikipedia and Wikisource (CC BY-SA 4.0).** Fifteen songs have score B notes copied from the LilyPond score on the English, German or Italian Wikipedia page or English Wikisource page named by the `score-b` URL. They are used under CC BY-SA 4.0, and that page is the attribution. They were retrieved on 2026-11-07. Only the notes are kept, with marks, chords, lyrics and other voices left out. They are used only to compare with score A and are never served: the stick gets only score A's notes.
- **Wikimedia Commons MIDI (CC0).** Winter, ade: score B is read from Peter Gerloff's MIDI file on Commons, which is released under CC0. It was retrieved on 2026-11-07, and it is used only to compare.
- **Blind readings (public domain).** Three Blind Mice and Weißt du, wie viel Sternlein stehen: score B is our own second reading of another print from before 1931, made without looking at score A. The print is public domain. The scan of the Weißt du print is marked for non-commercial use, so only the notes are kept, and only to compare.

This is the same arrangement as `tests/music/references.txt`, which holds the scores the built-in songs are compared with. A new song with a CC BY-SA score B needs the same first line.

### The legal evidence

A melody is either by named composers or traditional, never both.

| Key | What it holds |
|---|---|
| `composer` | One line per person who wrote the melody: `name \| born \| died \| url \| url`. Each must have died before 1926. The two URLs are two independent sources that agree on the dates, on different sites, such as Wikipedia and IMSLP. Two language editions of Wikipedia (`en.wikipedia.org`, `de.wikipedia.org`) are one site, Wikipedia. |
| `published` | `year \| url` of the first print, before 1931. Required with `composer`, optional with `traditional`. |
| `traditional` | Instead of `composer`, for a melody with no known author: `citation \| year \| url` of a print before 1931. One line per print. |

If a source mentions a late first edition, or the authorship is disputed, the song stays out. The tool can't see that, so check it before adding the song. The legal rule behind all this is in `docs/superpowers/specs/2026-11-07-song-library-design.md`, under "The legal rule".
