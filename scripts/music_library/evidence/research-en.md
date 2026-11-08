# Task 9 round 2, score A research: English songs

**Feasibility: GO for 5 drafts.** Mary Had a Little Lamb, Three Blind Mice and My Bonnie are drafted. Pop Goes the Weasel and Yankee Doodle are dropped. Hickory Dickory Dock and Humpty Dumpty replace them; both are J. W. Elliott tunes (1870).

## How the work was done

- **Logging.** Every transcription was logged in `transcripts.log` in this folder before any other score of that song was opened. No score B was opened for any song.
- **Reading the scans.** All scans are from the Internet Archive (`page/nN_w2400.jpg`), downloaded to scratch (`r2en/dl/`). I read them from 150–500% zooms, placing every head against the five lines. Three Blind Mice is a low-resolution page, so I also ran round 1's `heads2.py` pixel helper on it.
- **Self-test.** A scratch copy of the five drafts, with a placeholder score B made from my own notes and together with the four committed songs (no id or alias clash), passes `scripts/make_music_data.py`. That checks names, aliases, evidence fields, bar sums and range. In the real drafts the only open item is `score-b TODO`.
- **Transposition:** none for any song. Everything sits inside A3–A5 as printed. The lowest note is C#4, in Humpty Dumpty.
- **Independence.**
  - Wikipedia was read only through the `prop=extracts` plain-text API.
  - IMSLP was read only through work-page and category metadata (`|Copyright`, `|Year of First Publication` and similar lines). I opened no IMSLP score.
  - The open-score check is `tools/hasscore.sh` in scratch, which prints only yes, no or missing.
  - Other pre-1931 prints I saw are disclosed per song below. None of them is a score B candidate.

## Open score B: yes/no check

Wikipedia `<score>` and Mutopia search:

| Song | Open score B |
|---|---|
| Mary Had a Little Lamb | none found: en, de and fr Wikipedia have no score. Possible B: IMSLP "Goodnight Ladies (Christy)", CC BY 4.0 arrangement by Kelly, same tune |
| Three Blind Mice | none on Wikipedia (en/de/fr). Possible B: IMSLP "Three Blind Mice (English Folk Music)", CC BY 4.0, Messerschmidt 2019 |
| My Bonnie | yes, en.wikipedia `My_Bonnie_Lies_over_the_Ocean` |
| Hickory Dickory Dock | yes, en.wikipedia `Hickory_Dickory_Dock` |
| Humpty Dumpty | yes, en.wikipedia `Humpty_Dumpty` |
| Yankee Doodle (dropped) | no on en, fr, es, it Wikipedia, none on Mutopia; IMSLP only CC BY-NC-SA (Ludenhoff) |
| Pop Goes the Weasel (dropped) | yes, en.wikipedia |

## Songs drafted

### 1. three-blind-mice: Three Blind Mice

- **Scan:** "Three Blind Mice (Round)", No. 69, in *Twice 55 Community Songs* (The Brown Book), C. C. Birchard, Boston 1919. It is a single melody line.
  - Page: https://archive.org/details/twice-55-community-songs-no-1-1919/page/n43/
  - The IA item is marked Public Domain Mark 1.0.
- **Read:** 6/8 in D, no pickup, 16 bars, bar 12, the round sung once.
- **Flagged bars (6: bars 2, 4, 6, 8, 14, 16):**
  - **Resolution** (general, not counted). The page is served at only 1610 px wide, with a staff space of about 10 px. Pitches were measured with the helper and agree with the eye. Durations are by eye.
  - **Bars 2, 4, 6, 8 and 16.** The print has a dotted quarter tied to a quarter, then an eighth rest (bar 8: then the eighth "They"). Score B probably has one dotted half there, which the rest rule treats as equal.
  - **Bar 14** ("As"): the 4th note, A4, is read as a quarter with no flag visible, and the 5th, G4, as an eighth.
- **Evidence:** traditional.
  - Print: the 1919 Birchard print above (archive.org).
  - IMSLP "Three Blind Mice (English Folk Music)": Piece Style "Traditional (folk)", Year of First Publication 1609, Ravenscroft's *Deuteromelia*.
  - **Caveat:** Wikipedia and IMSLP both say the 1609 print is a minor-key form, not this tune. The major tune's documentation here is the 1919 print.
  - Wikipedia (https://en.wikipedia.org/wiki/Three_Blind_Mice) agrees: Roud 3753, 1609 Deuteromelia, in children's books from 1842 (Halliwell).
- **Also seen, not used:** *Our Old Nursery Rhymes*, Augener/McKay 1917, p. 11 (`ouroldnurseryrhy00lond`, page n20). I viewed only an overview of it.
- **Tempo** 120 (quarter; that is 80 to the dotted quarter). **Footer** "Twice 55 Community Songs 1919".

### 2. mary-had-a-little-lamb: Mary Had a Little Lamb

- **Which tune.**
  - Sarah Josepha Hale's words date from 1830.
  - Lowell Mason (1792–1872) set them in the 1830s. Wikipedia says Mason's tune "is not the one commonly sung today".
  - Today's tune is the "Merrily We Roll Along" chorus of "Goodnight, Ladies". Wikipedia attributes that song to Edwin P. Christy (1815–1862) and says it was first published on 16 May 1867. IMSLP gives "Year/Date of Composition 1847, First Publication 1867".
  - I transcribed today's tune, and it matches the "Merrily we roll along" contour note for note.
  - I filed it as **traditional**, because "attributed" is not a firm authorship. Every candidate author, Mason or Christy, died before 1926, so the legal answer doesn't change. **Lead may overrule** and file it as composer Christy.
- **Scan:** *The Most Popular Mother Goose Songs*, illustrated by Mabel Betsy Hill, Hinds, Noble & Eldredge, New York 1915, p. 12.
  - Page: https://archive.org/details/mostpopularmothe00hill/page/n15/
  - Copy: Boston Public Library.
- **Read:** 2/4 in F, voice staff only, verse 1, 8 bars, pickup 0.
- **Flagged bars (5: bars 1, 4, 5, 7 and the pickup; bar 8 is a note only):**
  - **Bars 1, 5 and 7 have a dotted figure** (A4:3 G4:1). Today's tune is usually even eighths, so a difference with score B is likely here and the user may need to settle it.
  - **Opening pickup.** The print opens with an A4 sixteenth pickup, which carries "And"/"It" of verses 2–6. Verse 1 starts on the downbeat, so the pickup is left out.
  - **Bar 4** ends with that same A4 sixteenth for later verses, which overfills the bar. It is left out.
  - **Bar 8** is F4 dotted quarter plus an eighth rest; written `F4:6 R:2`.
- **Evidence:**
  - Print: Hill 1915 (archive.org).
  - Wikipedia, `Goodnight,_Ladies`: 1867 print of the same tune; `Mary_Had_a_Little_Lamb`: Mason's tune is not today's.
  - IMSLP, `Goodnight_Ladies_(Christy,_Edwin_Pearce)`: 1847/1867.
- **Also seen, not used:**
  - *Our Old Nursery Rhymes* (Augener 1917) p. 7, page n12. It sets the words to a different tune in A without the "little lamb, little lamb" repeat, possibly Mason's.
  - *Traditional Nursery Rhymes* (Graham 1911): its OCR has it, but I didn't open it.
- **Tempo** 100. **Footer** "Most Popular Mother Goose 1915".

### 3. my-bonnie: My Bonnie Lies over the Ocean

- **Scan:** "My Bonnie" (Slow waltz time), in *Songs the Whole World Sings*, ed. Albert E. Wier, D. Appleton, New York 1915, p. 181.
  - Page: https://archive.org/details/songswholeworlds00wier/page/n184/
  - Copy: Harold B. Lee Library.
- **Read:** 3/4 in C, quarter pickup, verse 1 (16 bars) and chorus (16 bars), bar 12. The final bar `C5:4 R:4` closes against the pickup.
- **Flagged bars (3: bars 26, 31, 32; the verse/chorus voice choice is general):**
  - **Verse.** It is printed as two voices, and the upper voice (stems up) was taken.
  - **Chorus.** Bars 17–32 are piano chords, and the top note was read as the melody.
  - **Bar 26** ("back"): C5 is the top note, with its head displaced beside a Bb4 second.
  - **Bars 31–32:** C5 dotted half tied to a C5 quarter, then a quarter rest.
- **Authorship.**
  - Wikipedia calls the song traditional, Roud 1422. It also says Charles E. Pratt (1841 – 11 Aug 1902) published it in 1881 under the pseudonyms H. J. Fuller/Fulmer and J. T. Wood.
  - IMSLP lists "Bring Back My Bonnie to Me (Fulmer, H. J.)", T. B. Harms, New York 1882, with no dates for Fulmer.
  - I filed it as **traditional**, with prints from 1915 (archive.org) and 1882 (IMSLP). Pratt, if he is counted as the author, died in 1902, so the legal answer doesn't change. **Lead may overrule.**
- **Also seen, not used:** *The Golden Book of Favorite Songs*, Hall & McCreary, Chicago 1923, p. 121, page n116 (SATB, words "My Bonnie is over the ocean"). I viewed it before choosing this print. Its soprano agrees by eye in bars 1–4 and 21–25.
- **Tempo** 120. **Footer** "Songs the Whole World Sings 1915".

### 4. hickory-dickory-dock: Hickory Dickory Dock (replaces Pop Goes the Weasel)

- **Scan:** J. W. Elliott, "Dickory, dickory, dock" (Allegro), in *Mother Goose's Nursery Rhymes and Nursery Songs*, set to music by J. W. Elliott, engraved by Dalziel, McLoughlin Bros., New York, undated [187-?], p. 7.
  - Page: https://archive.org/details/mothergoosesnurs00elli/page/n14/
- **Read:** 6/8 in F, voice staff, 8 bars, pickup 0, bar 12.
- **Flagged bars (2: bars 6, 7; the staccato is general):**
  - **Staccato.** The print is staccato: eighths separated by eighth rests in bars 3, 5 and 6, and long rests in bars 2, 4 and 8. If score B holds those notes, the rest rule covers only the cases where the next onset matches.
  - **Bar 6:** the last note is D5, a dotted quarter.
  - **Bar 7:** C5 D5 C5 Bb4 A4 G4, read at 400%.
- **Evidence:** composer James William Elliott, 1833–1915.
  - https://en.wikipedia.org/wiki/James_William_Elliott ("1833–1915")
  - https://imslp.org/wiki/Category:Elliott,_James_William (born 13 Feb 1833, died 5 Feb 1915)
  - Hill's 1915 *Most Popular Mother Goose Songs*, p. 31 (page n34), credits the tune to "J. W. Elliott".
  - `published 1870` points to the scan, which IA dates only [187-?]. Elliott's *National Nursery Rhymes* is London 1870, per Wikipedia's Humpty Dumpty article. His death in 1915 is enough by itself under the spec.
- **Also seen, not used:** Augener 1917 p. 39 (page n44) and Hill 1915 p. 31 (page n34), as thumbnails.
- **Tempo** 132. **Footer** "Elliott, McLoughlin c. 1870".

### 5. humpty-dumpty: Humpty Dumpty (replaces Yankee Doodle)

- **Scan:** J. W. Elliott, "Humpty Dumpty" (Allegretto), in the same book, p. 30.
  - Page: https://archive.org/details/mothergoosesnurs00elli/page/n37/
- **Read:** 6/8 in D, voice staff, 10 bars, pickup 0, bar 12.
- **Flagged bars (3: bars 4, 8, 9):**
  - **Bar 4:** first note C#4, on a ledger line.
  - **Bar 8:** `F#4:6 G4:2 R:2 G4:2`, with an eighth rest inside the bar.
  - **Bar 9:** A4 dotted quarter joined to an A4 eighth on one syllable, read as a tie.
- **Evidence:** composer Elliott, as for song 4.
  - Wikipedia, Humpty Dumpty: "The melody commonly associated with the rhyme was first recorded by composer and nursery rhyme collector James William Elliott in his National Nursery Rhymes and Nursery Songs (London, 1870)".
  - **Caveat:** "first recorded" leaves open whether Elliott composed the tune or collected it. Either way it is pre-1926 and pre-1931. I filed it as composer Elliott. **Lead may overrule** and file it as traditional.
- **Tempo** 120. **Footer** "Elliott, McLoughlin c. 1870".

## Dropped

### Pop Goes the Weasel: no usable print of the known children's tune

- **Only print with music I reached:** Burchenal, *American Country-Dances* (G. Schirmer, New York 1918), p. 22, https://archive.org/details/americancountryd11burc/page/n41/.
  - It is a piano dance arrangement.
  - Bar 9 has a **triplet** grace figure. A triplet means rejection.
  - It is the dance variant, not the sung tune.
- **Other leads:**
  - Wolford, *The Play-Party in Indiana* (1916), has the "All around the American flag / Monkey chased the weasel" words, page n91. I didn't confirm that it prints music.
  - IMSLP's 1853 prints (Twiggs; Musical Bouquet) are dance and minstrel arrangements behind the captcha.
  - None of the children's songbooks I searched (Augener 1917, Hill 1915, Elliott, Golden Book 1923, Heart Songs 1909, Twice 55, Franklin Square) contains it.
- The legal side is clear (traditional, 1853 prints), so it could return with a better score A.

### Yankee Doodle: pre-1931 prints disagree, and there is no open score B

- **The prints disagree in bar 2** (degrees in the key):
  - *Our Old Nursery Rhymes* (Augener 1917, p. 45, page n50): 1 1 7 5.
  - *Golden Book* (1923, p. 8, page n7): 1 3 2 7.
  - *Heart Songs* (1909, p. 382, page n403): 1 3 2. 7, with a dotted rhythm.
  - None of them has the 1 3 2 5 that is usually sung today.
  - The choruses differ in rhythm too.
- **No open score B exists to settle it:** none on en, fr, es or it Wikipedia, none on Mutopia, and IMSLP has only a CC BY-NC-SA one.
- Whichever print was chosen, the user would have to settle bar by bar against a B that doesn't exist. No transcription was logged.
- The legal side is clear (traditional, Aird 1782 and others), so it can return if a CC BY or CC BY-SA score turns up.

## Flagged bars in total

19 flagged bars across the five drafts (unit: distinct bars; general notes not counted):

| Song | Flagged bars |
|---|---|
| Three Blind Mice | 6: bars 2, 4, 6, 8, 16 and 14 |
| Mary Had a Little Lamb | 5: bars 1, 5, 7, the pickup and bar 4 (bar 8 is only a note) |
| My Bonnie | 3: bars 26, 31 and 32 (the chorus-voice choice is a general note) |
| Hickory Dickory Dock | 2: bars 6 and 7 (the staccato rests are a general note) |
| Humpty Dumpty | 3: bars 4, 8 and 9 |

**Most likely to need a user decision:** Mary's dotted bars 1, 5 and 7, and the staccato rests in Hickory and Humpty.


## Score B

Score B was done after score A was finished, following `research-b-brief.md`.

- **Copied from text only.** The sources are Wikisource and Wikipedia `<score>` LilyPond, fetched as wikitext through the API, and a Commons MIDI turned into notes by a script. Nothing was read from an image.
- **No corrections to A.** A was not changed for any song, and there are no `settled` lines.

**Searched.**
- **Wikipedia:** every language edition, through langlinks, of Three Blind Mice, Mary Had a Little Lamb, Goodnight, Ladies, Hickory Dickory Dock, Humpty Dumpty and My Bonnie Lies over the Ocean.
- **en.wikisource**, all namespaces: `<title word> insource:/\<score/` for Humpty, Dumpty, Dickory, mice, lamb, Mary, Merrily and Bonnie. This finds the proofread *Child's Own Music Book*, ed. Albert E. Wier, Mumil Publishing, New York 1918 (`Index:Childs own music book.djvu`).
- **en.wikibooks:** the same searches, nothing.
- **Mutopia:** mice, lamb, Bonnie, dock, Humpty, Merrily, Ladies and Elliott, nothing.
- **Commons:** files titled with each song name.
- **IMSLP:** work-page wikitext for the two CC BY arrangements. Both offer only PDFs, with no LilyPond or MusicXML.

**Scratch.** Everything is under `<scratchpad>/scoreb-en/`:
- `lib/` and `out/`.
- `tools/bars.py`: bar-by-bar comparison through the tool's own `read_score`, `join` and `fold_rests`.
- `tools/rel2abs.py`: resolves `\relative` octaves by LilyPond's rules and keeps the top note of each chord.
- `tools/raw.py` and `tools/unround2.py`: the MIDI tools.
- `crops/`: crops of A's My Bonnie scan.

| Song | Result |
|---|---|
| Hickory Dickory Dock | **PASS** (Wier 1918 on Wikisource) |
| Mary Had a Little Lamb | **DIFF**: bar 4 (B keeps the verse-2 sixteenth), bar 7 (B even) |
| Three Blind Mice | **DIFF**: B (Commons MIDI) is the 12-bar round; bars 8 and 13 rhythm; final length |
| My Bonnie | **DIFF**: bars 1/5/9 dotted against even; bars 20 and 28; final length |
| Humpty Dumpty | **NEEDS BLIND READING**: every open score found is a different tune |

Bar numbers are A's. Lengths are in sixteenths, after ties are joined and each rest is folded into the note before it, which is how the tool compares.

### 1. hickory-dickory-dock: PASS

- **B:** https://en.wikisource.org/wiki/Page:Childs_own_music_book.djvu/52, "DICKORY, DICKORY, DOCK" (Allegro, 6/8, F).
  - **Licence:** the Wikisource transcription is CC BY-SA 4.0. The book is Wier, New York 1918, a pre-1931 print.
  - The page is at proofread level 4 (validated).
  - It is a different edition and engraving from A's Elliott/McLoughlin print, and the tune is the same.
- **Copied:**
  - The upper staff of the piano score, `\relative c''`, was run through `rel2abs.py`. That resolves the octaves, keeps each chord's top note, and drops staccato marks and dynamics.
  - The result is written into `score-b` as absolute LilyPond, with `|` added at the bar lines.
  - `q` (repeat chord) takes the previous chord's top note.
  - The top voice carries the lyric line, which matches the melody, note for note.
- **Result:** all 8 bars agree with no transposition. A's flagged bars 6 (`Bb4:2 R:2 Bb4:2 D5:6`) and 7 (`C5 D5 C5 Bb4 A4 G4`) agree.
  - B's staccato rests sit where A's do.
  - In bar 5, B's `C5:2 R:2 C5:2` folds to A's `C5:4 C5:2`.
- **Wikipedia's score is not this tune.** en and ca have the same score, opening `fis8 g a a b cis | d4.~ d4 a8`. That is scale degrees 3-4-5-5-6-7 | 1, against Elliott's 3-4-5-4-3-2 | 3, and the phrases never realign. **For the user:** it is the modern tune, so Elliott's 1870 tune may not be the one children know.

### 2. mary-had-a-little-lamb: DIFF

- **B:** https://en.wikisource.org/wiki/Page:Childs_own_music_book.djvu/57, "MARY HAD A LITTLE LAMB" (Andantino, 2/4, F, the same key and metre as A).
  - **Licence:** the Wikisource transcription is CC BY-SA 4.0. The book is Wier 1918, a pre-1931 print and a different edition from A's Hill 1915.
  - **Caveat:** the page is at proofread level 2 ("problematic"); the page also holds Yankee Doodle.
- **Copied:**
  - The upper staff, `\relative c''`, was run through `rel2abs.py` for the top notes.
  - **Left out:** B's opening bar `s4. s16 a16`, a spacer bar ending in the A4 sixteenth pickup. Verse 1's lyric line gives that note no syllable ("_ Ma -- ry"), and verse 2 sings "And" on it. This is the same pickup A leaves out, for the same reason.
  - **Padded:** B's last bar, `f4. r16`, is one sixteenth short only because it balanced that pickup, so `r16` was appended (brief rule 3).
  - **Kept as printed:** the A4 sixteenth at the end of bar 4 (`c''8. a'16`). It lies inside the bar, and verse 1's lyric line runs "lamb" over it (`lamb, __ _`).
- **Exact differences:**

| A bar | A (Hill 1915) | B (Wier 1918) | Note |
|---|---|---|---|
| 4 | `A4:2 C5:2 C5:4` | `A4:2 C5:2 C5:3 A4:1` | A's print has the same A4 sixteenth ("And" of verse 2) after the bar's C5, overfilling the bar, and A left it out. B prints it inside the bar. |
| 7 | `G4:2 G4:2 A4:3 G4:1` | `G4:2 G4:2 A4:2 G4:2` | B is even here. |

- **A's dotted bars 1 and 5 agree with B,** `A4:3 G4:1 F4:2 G4:2`. Bars 2, 3, 6 and 8 agree as well.
- **Other open scores of this tune.** Both are in 4/4 at doubled values, which the tool cannot scale, so neither was stored. The comparisons below halve them in scratch.
  - **it.wikipedia** `Mary_had_a_little_lamb` `<score>`, CC BY-SA 4.0, English lyrics: even eighths in bars 1, 5 and 7, otherwise the same as A.
  - **ja.wikipedia** `メリーさんのひつじ` `<score>`, CC BY-SA 4.0: dotted in bars 1 and 5 like A, even in bar 7, and a rest in place of the last A4 in bar 6 (Japanese words).
  - **Bar 7:** none of the three open scores has A's dotted figure there.
- **Not usable:**
  - IMSLP "Goodnight Ladies (Christy)" has only Coward's ATBarB arrangements, edited by Kelly (CC BY 4.0), as PDFs.
  - Commons `File:Mary Had a Little Lamb.ogg` is an audio recording.

### 3. three-blind-mice: DIFF

- **B:** https://commons.wikimedia.org/wiki/File:Three_Blind_Mice_round.mid.
  - **Licence:** CC BY-SA 3.0 / GFDL (`{{self|cc-by-sa-3.0|GFDL}}`, "own work" by User:Hyacinth, 2012). The tune is tagged `{{PD-old}}`.
  - It is in 6/8 and D, at A's pitch.
- **Copied by script:**
  - `raw.py` dumps the note-on and note-off events.
  - Track 1 holds the melody M plus M delayed 24 sixteenths (entries 1 and 2). `unround2.py` assigns each event to the delayed copy if M has the same event 24 earlier, and otherwise to M. Times are rounded to the sixteenth; two off-events sit at 118.078 and 119.891.
  - Every event was accounted for, with no notes left open. Track 2 holds entries 3 and 4, 48 later, and gives the identical M.
  - M is 138 sixteenths, on the grid, with no chords. It is written into `score-b` as absolute LilyPond.
  - The last note ends half a bar early. There is no pickup to balance it, so no rest was appended.
- **Differences** (B is 12 bars, A 16):
  - **Form.** B sings "Three blind mice" (A bars 1–2) and "See how they run" (A 5–6) once each. A repeats both (bars 3–4 and 7–8).
  - Aligned, B 1–2 = A 1–2, B 3 = A 5/7, B 5–8 = A 9–12, B 10 = A 14 and B 11 = A 15 all agree. The remaining differences:

| A bar | A | B | Note |
|---|---|---|---|
| 8 (B 4) | `F#4:10 A4:2` | `F#4:4 R:4 A4:4`, folded `F#4:8 A4:4` | "They" comes as a quarter, an eighth earlier. |
| 13 (B 9) | `D5:2 D5:2 D5:2 C#5:2 B4:2 C#5:2` | `D5:4 D5:2 C#5:2 B4:2 C#5:2` | |
| 16 (B 12) | `D4:12` (`D4:10 R:2`) | `D4:6`, end | |

- **A's flagged bar 14 agrees with B:** `D5:2 A4:2 A4:2 A4:4 G4:2`.
- **Third witness, not stored:** Wier 1918 on Wikisource, https://en.wikisource.org/wiki/Page:Childs_own_music_book.djvu/56 (level 4), piano score.
  - Its plain parts, with octaves resolved by `rel2abs.py`, give a third variant: `F#4:6 E4:6 | D4:12 | F#4:6 E4:4 E4:2 | D4:6 D4:4 A4:2`.
  - It is 12 bars, sings "See how they run" on F#–E–E–D, and leaves out A's A4–G4 phrase entirely.
  - Its bars 5–10 are in two-voice `<< \\ >>` blocks, which I did not resolve by script, so they are not compared.
  - So no open text score found has A's 16-bar form.
- **If the user wants a B with A's form, it NEEDS BLIND READING** of one of these:
  - IMSLP "Three Blind Mice (English Folk Music)", Messerschmidt, CC BY 4.0, PDF only: https://imslp.org/wiki/Three_Blind_Mice_(English_Folk_Music). For the reader: take one instrument's part once through, not the top voice.
  - *Our Old Nursery Rhymes*, Augener/McKay 1917, p. 11, a pre-1931 print: https://archive.org/details/ouroldnurseryrhy00lond/page/n20/.

### 4. my-bonnie: DIFF

- **B:** https://en.wikipedia.org/wiki/My_Bonnie_Lies_over_the_Ocean, `<score>`, CC BY-SA 4.0. The page and markup name no source edition.
  - The `melody` block is copied verbatim, both `\repeat volta 2` blocks with their `\alternative`s, using `\relative g'`.
  - Dropped: `\global` (key G, 3/4, `\partial 4`), `\set` and `\bar`.
  - It moves by +5.
- **Corrections to A: none.** A's own scan, `r2en/dl/sw_n184.jpg` (2533×3388), was cropped and enlarged 3×. The crops are `crops/b20x3.jpg` and `crops/b28x3.jpg`.
  - **Bar 20 ("back,").** The staff lines are at y ≈ 100/168/238/308/380 (F5/D5/B4/G4/E4). The heads sit at ledger C4, the F4 space, the A4 space and the C5 space (≈ 200). The top note is **C5**, as A has it; D5 would sit on the line at 168.
  - **Bar 28 ("back, Oh,").** The lines are at y ≈ 148/218/290/360/432. The half note's top head is on line 2 (≈ 220), **D5**. The quarter's is in the space below (≈ 250), **C5**. A's `D5:8 C5:4` is confirmed.
  - **Bars 1, 5 and 9.** A dotted quarter plus a flagged eighth ("Bon-nie lies"), as A has it.
- **Exact differences:**

| A bar | A (Wier 1915) | B (en.wikipedia) |
|---|---|---|
| 1, 5, 9 | `E5:6 D5:2 C5:4` | `E5:4 D5:4 C5:4` |
| 20 | `C5:12` (top of the chord C4 F4 A4 C5) | `D5:12` |
| 28 | `D5:8 C5:4` | `D5:12` (B repeats one chorus body, so bars 20 and 28 are the same) |
| 31–32 | `C5:12~ C5:4 R:4`, 20 to the end, closing against the pickup | `g2.~ g2.`, 24 |

  - In bars 31–32, B's last bar is full despite the pickup, so B is 4 longer overall (388 against 384). Only padding B is allowed, not trimming it.
  - Everything else agrees.

### 5. humpty-dumpty: NEEDS BLIND READING

- **No open text score of Elliott's tune.**
  - Wikipedia en, ar, ca, de, ko and ro all carry the same score: B flat, 6/8, 8 bars, `d4 f8 es4 g8 | f8 g a bes4.`, degrees 3-5-4-6 | 5-6-7-1.
  - Commons `File:MotherGooseHumptyDumpty.mid` (after Walter Crane, *Mother Goose's Nursery Rhymes*, Routledge 1877, p. 42; tagged PD-old-70) is that same common tune. Converted by script: `E4 G4 F4 A4 | G4 A4 B4 C5`.
  - A (Elliott, McLoughlin p. 30) opens 5-1'-3-5 | 1-2-3-2 and has 10 bars, so it never realigns with either.
  - Wikisource has no Humpty `<score>`.
- **Contradiction to flag.**
  - en.wikipedia says the "melody commonly associated with the rhyme was first recorded by … Elliott in his National Nursery Rhymes (London, 1870), as outlined below", and then prints the common tune.
  - Elliott's own print (A), and Novello's 1872 London edition with the same engraving, have a different tune.
  - So the draft's comment ("which Wikipedia names as the melody commonly sung") misdescribes A's tune, and `published 1870 | https://en.wikipedia.org/wiki/Humpty_Dumpty` points at a page showing another tune.
  - The legal answer doesn't change: Elliott died in 1915 and this is his print. The lead should fix the wording, and the user should know children sing the other tune.
- **Candidate scans for the blind reader:**
  - **Preferred: an independent engraving.** *The Most Popular Mother Goose Songs*, ill. Hill, Hinds, Noble & Eldredge, New York 1915, a pre-1931 print: https://archive.org/details/mostpopularmothe00hill/page/n28/. The header reads "HUMPTY DUMPTY", with a credit at the right that looks like "J. W. Elliott" at thumbnail size. The printed page is probably p. 25: full-text search puts it at leaf 29, and the leaf map gives leaf 34 as p. 30.
  - **Alternative: the same Dalziel plates as A.** Elliott, *National Nursery Rhymes and Nursery Songs*, Novello, Ewer & Co. / Routledge, London 1872, p. 30, a pre-1931 print (Boston Public Library copy): https://archive.org/details/nationalnurseryr00elli/page/n45/. The contents page lists "Humpty Dumpty … 30", and the thumbnail shows the title.

### Legal fields

The tool reports no legal problem for any of the five: composer URLs, `published` and `traditional` lines all pass. Hickory passes the tool completely.

### Final tool output

Run from the repo root: `python3 -I -B scripts/make_music_data.py --library <scratch>/lib --out <scratch>/out`. Humpty still has `score-b TODO`.

```
Nothing written. Failed:
  humpty-dumpty: score-b must be: url | language | relative <pitch> or absolute | LilyPond notes
  mary-had-a-little-lamb: score B differs from score A at event 13 (MIDI/sixteenths, B moved by +0), after joining ties and folding rests: A 72/4 69/3 67/1 65/2, B 72/3 69/1 69/3 67/1; fix the notes, or add a settled line saying why a person accepts the difference
  my-bonnie: score B differs from score A at event 2 (MIDI/sixteenths, B moved by +5), after joining ties and folding rests: A 76/6 74/2 72/4 74/4, B 76/4 74/4 72/4 74/4; fix the notes, or add a settled line saying why a person accepts the difference
  three-blind-mice: score B differs from score A at event 4 (MIDI/sixteenths, B moved by +0), after joining ties and folding rests: A 66/6 64/6 62/12 69/6, B 69/6 67/4 67/2 66/8; fix the notes, or add a settled line saying why a person accepts the difference
exit 1
```
