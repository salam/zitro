# Task 9, research A: evergreens

**Feasibility: GO for 5 drafts.** Four of the five assigned songs are drafted. Santa Lucia was dropped because its authorship is disputed. "Long, Long Ago" (Bayly) replaces it.

All five drafts were read from pre-1931 scans on the Internet Archive. The only open item in each is `score-b TODO`.

**Unsure bars, 17 in all:** Camptown Races 8, Greensleeves 3, Londonderry Air 2, Funiculì 3, Long, Long Ago 1. Bars that are expected to differ from score B but were read with confidence are listed separately under each song and not counted.

## How the notes were read

- **Order of work:** each transcription was logged to `transcripts.md` in this folder before any other score was consulted. One later correction, re-read from the same scan, is logged as an amendment (Funiculì, bar 48).
- **Tools:** pitches were measured against the staff lines with small scratch helpers. Rhythms, rests, ties and accidentals were read by eye from zoomed crops. The helpers, written in scratch and not in this folder, are:
  - `pitch.py` and `pitchloc.py`: head finder, with global or local staff lines.
  - `chord.py`: a darkness profile per column. It reads stacked chord heads and hollow heads.
- **Range:** all five songs are in range as printed (MIDI 57–81), so none is transposed.
- **Self-test:** a scratch copy of the five drafts plus the four committed songs passes `scripts/make_music_data.py`. For that run only, score B was a fake copy of A made by `fakeb.py`, in scratch. The tool accepts the names, aliases, evidence, bar lengths and range.
- **Open-score check:** I did not run the yes/no check for open scores, because the Wikipedia API stopped answering JSON at that point. Nothing of any score B was seen.

### Independence disclosures

None of the scores below is a likely score-B candidate. All of them are old prints.

- **Funiculì:** before choosing my print I opened the Ricordi album pages (Napoli 1919, pp. 34–38, `raccoltadicelebr00unse` n39–n43) and an overview of George Rosey's male-voice arrangement (*College Songs*, Remick 1909, pp. 56–57, `collegesongs00rose` n61–n62). I then transcribed only from the Scottish Students' Song Book.
  - I dropped Ricordi as score A for two reasons. Its catalogue date of 1919 is uncertain (plate 127633, "Tutti i diritti riservati"). And it has the Italian words.
- **Greensleeves:** I looked at both of Chappell's copies, which are in the same print, before choosing one (see below).
- No Wikipedia score, LilyPond, ABC, MIDI or IMSLP file was opened. I read Wikipedia only through the plain-text extract API, and IMSLP only through wikitext metadata fields. The Internet Archive items `imslp-camptown-races-foster-stephen` and `imslp-funicul-denza-luigi` were not opened. Their file list shows a modern band edition and Jan Van Duffel's CC BY 3.0 arrangement, which is a possible score B for Funiculì.

## 1. camptown-races: Camptown Races

- **Scan:** *Heart Songs Dear to the American People*, Chapple Publishing, Boston 1909, pp. 352–354:
  - https://archive.org/details/heartsongsdearto00chap/page/n373/
  - `/page/n374/`
  - `/page/n375/`
- **Read:** 2/4, E major, Allegro vivace, pickup one eighth. Verse 1 (16 bars) and chorus (8 bars), 24 bars in all.
  - In the verse, the melody is the vocal staff. The chorus is printed in chords, so its melody is the top note.
  - Where a slurred pair of sixteenths sets one syllable it is one note, and where it sets two syllables it is two notes, following verse 1.
- **Evidence:**
  - Composer: Stephen Foster, 1826–1864.
    - https://en.wikipedia.org/wiki/Stephen_Foster
    - https://imslp.org/wiki/Category:Foster,_Stephen
    - Library of Congress n50023175 agrees.
  - Published February 1850 by F. D. Benteen: https://en.wikipedia.org/wiki/Camptown_Races. The IMSLP work page also gives 1850.
- **Transposition:** none. The range is E4–E5.
- **Re-read check:** after the first transcription, every head was re-measured with `chord.py`, the darkness profile. Zooms confirmed that bar 2 is C#5 (a space note, the B4s sit on the line), that the chorus top notes are right, and that the tied endings are right. No note changed.
- **Unsure bars (8):** 1, 5, 6, 9, 13, 14, 21 and 23. These are all the same question: whether a slurred pair of equal sixteenths is one note or two.
  - I followed the verse 1 syllables. Bars 1, 5 and 6 merge a pair ("la-dies", "five miles"). Bars 9, 13, 14, 21 and 23 split pairs into sixteenths: `B4:1 B4:1`, `C#5:1 C#5:1 B4:1 B4:1`, `G#4:1 G#4:1 B4:1 B4:1` and `F#4:1 F#4:1`.
  - A score B with other words may differ here.
- **Expected to differ (not counted):**
  - **Bar 16:** the verse ends `E4:6 R:2` before the chorus.
  - **Bar 18:** `E5:4~ E5:2 R:2` is written as `E5:6 R:2`.
  - **Bar 24:** the closing `E4:4~ E4:2` is written as `E4:6`.
- **Aliases:** `de camptown races`, `gwine to run all night` (Foster's subtitle), `camptown ladies`.
- **Tempo** 112. **Footer:** "Heart Songs, Chapple 1909".

## 2. greensleeves: Greensleeves

- **Scan:** W. Chappell, *Popular Music of the Olden Time*, vol. 1, Cramer, Beale & Chappell, London [1855–1859]. The Internet Archive item dates it 1855.
  - p. 230, "Tune of Green Sleeves. Oldest copy.": https://archive.org/details/popularmusicov1185559chap/page/n277/
- **Which copy:** Chappell prints two, and I looked at both before choosing.
  - The "Later copy" on p. 233 (`/page/n280/`, "Boldly", words of *The Trimmer*) is a dotted dance version. Its second part opens on high chords ("I'm first on the wrong side"), which is not the familiar refrain.
  - The "Oldest copy" on p. 230 has the familiar shape. The first part is E | G A B. C B | A F# D. E F#. The refrain is D. D C# B | A F# D. E F#, with the raised C# and D#. So it is the one drafted.
- **Read:** 6/8, one sharp (E minor), "Smoothly and in moderate time", pickup one eighth. Two eight-bar parts, 16 bars in all. The melody is the top note of the right hand.
- **Evidence (traditional):**
  - Chappell's print above, 1859: https://archive.org/details/popularmusicov1185559chap/page/n277/
  - Set to the carol "What Child Is This?" in 1871: https://en.wikipedia.org/wiki/What_Child_Is_This%3F
  - The Greensleeves article (https://en.wikipedia.org/wiki/Greensleeves) gives the 1580 Stationers' registration and the late 16th-century lute sources.
  - hymnary.org/tune/greensleeves says "English ballad, 16th cent., Public Domain", but it names no dated print.
  - The "What Child Is This?" article says "possibly by Richard Jones", a 16th-century printer. That is no legal concern.
- **Transposition:** none. The range is B3–D5. B3 is MIDI 59.
- **Unsure bars (3):**
  - **Bars 4 and 12 (B3):** a ledger-line note under chord heads. Bar 4 is `B3:4 E4:2` and bar 12 is `B3:6`.
  - **Bar 8 (`E4:6 E4:4 R:2`):** an eighth rest completes the bar before the second part, which starts on the downbeat.
- **Expected to differ (not counted):**
  - **Bar 16 (`E4:6 E4:4`):** shortened to 10 sixteenths to close against the pickup.
  - **Bars 7 and 15** (`G4:3 F#4:1 E4:2 D#4:3 C#4:1 D#4:2`): clear in the print, but a modern score may write the turn differently.
- **Aliases:** `green sleeves`. "What Child Is This" was left out on purpose, as it is a different song title. Add it if wanted.
- **Tempo** 96, a dotted quarter of 64. **Footer:** "Chappell, Olden Time 1859".

## 3. londonderry-air: Londonderry Air / Danny Boy (tune only)

- **Scan:** G. Petrie, *The Petrie Collection of the Ancient Music of Ireland*, vol. 1, M. H. Gill for the Society for the Preservation and Publication of the Melodies of Ireland, Dublin 1855, p. 57, "Name unknown", the air from Miss J. Ross of Limavady.
  - https://archive.org/details/petriecollection01petr/page/n84/
- **Why Petrie:** every later version descends from this print, and no later hand is in it. The 1913 Boosey "Danny Boy" sheet was not used: Weatherly died in 1929, and if that edition changed notes it would bring him into the melody.
- **Read:** C (4/4), three flats, Andante, "crotchet = pendulum 24 inches" (about 77), pickup three eighths, 16 bars. The melody is the stems-up top voice of the right hand. Fermatas and staccato dots are ignored. There are no grace notes, triplets or thirty-seconds.
- **Evidence (traditional):**
  - Petrie 1855: https://archive.org/details/petriecollection01petr/page/n84/
  - https://en.wikipedia.org/wiki/Londonderry_Air: published by Petrie in 1855 as an anonymous air; Weatherly's words were set to the tune in 1913.
  - hymnary.org returned 403.
- **Transposition:** none. The range is B3–G5.
- **Unsure bars (2):**
  - **Bar 13 (climax):** `G5:6 F5:2 F5:2 Eb5:2 C5:2 Eb5:2`, with the high notes on ledger lines and chord heads below.
  - **Bar 15 (close):** `G4 C5 Bb4 G4 F4 Eb4 C4 D4`, eight eighths over dense chords.
- **Expected to differ from a "Danny Boy" score B (not counted):**
  - **Bar 6:** `F4 Eb4 C4 B3 | C4 D4 Eb4 F4`. The natural sign before the low B was confirmed in a zoom, so the note is B natural. Later sheets may have Bb or another turn.
  - **Bars 2, 4, 8, 10, 12 and 14:** Petrie has an eighth rest after the long note. A held note in score B still compares equal if the next onset matches.
  - **Bars 13 and 15:** Danny Boy sheets often vary here.
  - **Last bar:** `Eb4:8~ Eb4:2`, as printed (a half note tied to an eighth). It closes against the pickup.
  - **Last bar:** `Eb4:8~ Eb4:2`, as printed (half note tied to an eighth). It closes against the pickup.
- **Names:**
  - `de` is "Londonderry Air" and `en` is "Danny Boy".
  - Alias: `oh danny boy`. "londonderry air" is not repeated as an alias, because it already equals `de`.
- **Tempo** 76. **Footer:** "Petrie, Dublin 1855".

## 4. funiculi-funicula: Funiculì, Funiculà

- **Scan:** *The Scottish Students' Song Book*, Bayley & Ferguson, London 1897, pp. 236–239, "Funiculi, Funicula. Words by Edward Oxenford. Music by L. Denza." A footnote reads "Included by arrangement with Messrs. G. Ricordi & Co., Milan."
  - p. 236: https://archive.org/details/scottishstudents00scot_0/page/n241/
  - p. 237: `/page/n242/`
  - p. 238: `/page/n243/`
  - p. 239: `/page/n244/`
- **Read:** 6/8, E flat major, Allegretto brillante, pickup one eighth, 52 bars.
  - **Verse 1:** the solo line plus the chorus echoes that are printed in the same vocal staff ("And so do I!", "To pine and sigh!", "Some joyous song!", "Is far from wrong!").
  - **Refrain:** the solo refrain "Listen! Listen! Echoes sound afar! … Tra la la … la!", up to p. 239, system 2, bar 1.
  - **Left out:** the chorus repeat of the refrain in two-part harmony that follows, and the 1st/2nd/3rd-time endings. The last bar is `Eb4:2` plus rests, shortened to close against the pickup.
- **Words:** the English underlay (Oxenford) and the Italian (Ricordi) carry the same notes in the bars compared. These are the verse bars "Some think the world is made for fun" and "-ra, Nannine, me ne sagliette, Tu saie addò", which I noticed on the Ricordi page before choosing my print.
- **Evidence:**
  - Composer: Luigi Denza, 1846–1922.
    - https://en.wikipedia.org/wiki/Luigi_Denza (born 24 February)
    - https://imslp.org/wiki/Category:Denza,_Luigi (born 23 February)
    - The years agree. Only the day of birth differs, which does not block.
  - Published 1880, Ricordi, plates 47126 and 47127: https://imslp.org/wiki/Funicul%C3%AC,_Funicul%C3%A0_(Denza,_Luigi). The Wikipedia article agrees.
  - The words are by Peppino Turco (died 1903, IMSLP gives 1907) and, in English, by Oxenford. They don't count.
- **Transposition:** none. The range is D4–F5.
- **Unsure bars (3), counting the pickup as bar 0:**
  - **Bars 20–21:** A natural in E flat ("I love to spend my time in"). Each bar has a printed natural on its first A. The second A, which has no sign, is read as natural too.
  - **Bar 40** ("far!"): `D5:6~ D5:2`. The slur between two D5s is read as a tie.
- **Expected to differ from score B (not counted):**
  - **Bar 48** ("la!", ten.): Eb5:12. It was first logged as C5 and corrected by a darkness profile of the hollow head (see the amendment in `transcripts.md`), so it is now measured.
  - **Bar 36:** a whole bar of rests (`F4:2 R:2 R:2 R:4 R:2`) before "Listen!" starts on the downbeat.
  - **Bars 37, 38, 41 and 42:** "Listen!" is written `D5:6 C5:2 R:2 R:2`. Score B may give the eighth a different length.
  - **Bars 4, 13, 22 and 31:** `X:6 X:6~` over the bar line ("fro-lic", "sing-ing").
  - **Chorus echoes:** an Italian score B may differ where the English underlay splits or merges notes.
- **Aliases:** `funiculi`, `jamme jamme`. The full title normalises to "funiculi funicula".
- **Tempo** 120, a dotted quarter of 80. **Footer:** "Scottish Students' Song Book 1897", which is 33 bytes.

## 5. long-long-ago: Long, Long Ago (substitute)

- **Scan:** *Heart Songs Dear to the American People*, Chapple Publishing, Boston 1909, p. 435, "Long, Long Ago, T. H. Bayly".
  - https://archive.org/details/heartsongsdearto00chap/page/n452/
- **Read:** 4/4, F major, no pickup, 16 bars, verse 1. The melody is the top note of a four-part setting, read with `chord.py`. Fermatas are ignored.
- **Evidence:**
  - Composer: Thomas Haynes Bayly, 1797–1839.
    - https://en.wikipedia.org/wiki/Thomas_Haynes_Bayly
    - https://imslp.org/wiki/Category:Bayly,_Thomas_Haynes
    - Library of Congress n79055355, "Bayly, Thomas Haynes, 1797-1839", agrees. Its record no00035822 cites Fuld: "words and music by Thomas Haynes Bayly".
  - Published 1839 | https://imslp.org/wiki/Long,_Long_Ago_(Bayly,_Thomas_Haynes). IMSLP lists a Cramer, Addison & Beale edition "n.d. [1839]" and gives "Year of First Publication=1835 ?".
- **For the user to settle:** https://en.wikipedia.org/wiki/Long,_Long_Ago says it was written in 1833 and "first published, posthumously, in a Philadelphia magazine", popular by 1844.
  - That is a first print after Bayly's death, but only about four years later and long before 1931. I don't read it as the "late first edition" the rule guards against.
  - If you read the rule more strictly, drop this song too.
- **Transposition:** none. The range is C4–D5.
- **Unsure bars (1):** bar 5, first note F4. Its head is faint in the scan, but it matches bars 1 and 13. All chord tops were measured with the darkness profile.
  - All chord tops were measured with the darkness profile.
  - The two low C4 eighths in bars 9 and 11 ("all my", "that so") are single ledger-line heads in the melody.
- **Names:**
  - `de` "Lang, lang ist's her" is the German version. Library of Congress records a German edition "Lang ist's her".
  - `en` "Long, Long Ago".
  - Aliases: `lang lang ist es her`, `lang ists her` (the form the Library of Congress record proves), `the long ago` (the original title). The doubled "lang" in `de` is the commonly sung German title, but no source I fetched prints it.
- **Tempo** 88. **Footer:** "Heart Songs, Chapple 1909".

## Dropped and not taken

- **Santa Lucia (Cottrau): dropped, because the authorship is disputed.**
  - **The sources disagree:**
    - Wikipedia (https://en.wikipedia.org/wiki/Santa_Lucia_(song)) calls it a traditional Neapolitan song, with Cottrau the transcriber and translator "often miscredited as its composer". It adds that "various sources credit A. Longo with the music, 1835".
    - IMSLP (https://imslp.org/wiki/Santa_Lucia_(Cottrau,_Teodoro)) cites Fuld: Cottrau "only notated and arranged" it. It guesses that "A. Longo" is Achille Longo (1832–1919), which doesn't fit a tune from 1835.
  - **Why the format fails:** a `composer` line fails because the sources disagree, and a `traditional` line conflicts with a named attribution.
  - **Why it is still safe to restore:** every candidate died before 1926, and it was first printed in Naples in 1849 (B. Girard & Co., plate 9640). So you could bring it back as traditional if you choose.
  - **Scans already found:**
    - Ricordi, *Raccolta di celebri canzoni napoletane*, Napoli [1919], pp. 26–27: https://archive.org/details/raccoltadicelebr00unse/page/n31/ and `/page/n32/`
    - *Heart Songs* 1909, p. 333, `heartsongsdearto00chap` around n354
    - Schirmer, *Songs of Italy*, 1904, No. 44 (`songsofitalysixt00marz`)
- **"Home! Sweet Home!" (Bishop): not taken.** Bishop's dates disagree:
  - Wikipedia gives 18 Nov 1787 – 30 Apr 1856.
  - IMSLP and the Library of Congress give 1786–1855.
  - Bishop's own "Sicilian air" story also adds doubt.
- **"La Paloma":** not checked. Wikipedia mentions an 1879 registration in Madrid, 14 years after Iradier's death, so it would need care.

# Score B (Task 9, round 2, research B)

| Song | Result | Score B |
|---|---|---|
| camptown-races | **DIFF** (bar 23 rhythm; last note length) | en.wikisource, Benteen 1850 first edition, CC BY-SA |
| greensleeves | **PASS** | en.wikisource, Chappell p. 230 (same print as A), CC BY-SA |
| londonderry-air | **PASS** | en.wikipedia, Petrie 1855 (same print as A), CC BY-SA |
| funiculi-funicula | **DIFF** (trailing rest only) | it.wikipedia, CC BY-SA |
| long-long-ago | **NEEDS BLIND READING** | no copyable open score |

No corrections to score A. No `settled` lines. Each B was copied from text (LilyPond in a `<score>` tag), never read from an image. Only the melody notes were copied. Lyrics, marks, dynamics, beams, bar checks, `\break` and `\bar` were dropped, and a tie written `x ~ y` was written `x~ y`.

## camptown-races: DIFF

- **Score B:** https://en.wikisource.org/wiki/Page:Camptown_Races_(1850).pdf/2
  - A LilyPond transcription of the first edition: "Gwine to Run All Night, or De Camptown Races", "Entered … 1850 by F. D. Benteen … District Court of Md."
  - Wikisource text is CC BY-SA 4.0, and the print is public domain. The page is proofread, level 3.
  - The print is a different edition from A's (*Heart Songs*, 1909).
- **Part taken:** the top vocal staff, `\new Voice="a"`. It holds the SOLO lines, the CHORUS echoes and the chorus top voice "Gwine to run all night", which is the tune.
  - Left out: that voice's rests during the piano introduction (`s2` ×5, `R2 R2`, `r4 r8`), the two lower chorus staves, the bass and the piano.
  - B is in D major, and the tool moves it +2.
- **Result: DIFF.** There are two differences.
  1. **Bar 23 ("Some-bo-dy bet on de"):**
     - A: `F#4:2 G#4:1 A4:1 G#4:2 F#4:1 F#4:1`.
     - B: `e8 fis16 g fis e8 e16`, which is `F#4:2 G#4:1 A4:1 G#4:1 F#4:2 F#4:1` after the +2 move. In B, "bet" is a sixteenth and "on" an eighth. In A it is the other way round.
     - Tool: `A 68/2 66/1 66/1 64/6, B 68/1 66/2 66/1 64/8` (event 77).
     - **A's scan re-checked, so A stands.** I cropped p. 354, system 1, bar 3 from `heartsongsdearto00chap/page/n375_w2000.jpg` (x 1640–2460, y 300–720). The stems of "Some, bo, dy, bet, on, de" carry 1, 2, 2, 1, 2, 2 flags, so the rhythm is eighth, sixteenth, sixteenth, eighth, sixteenth, sixteenth. That is A's reading.
     - The two prints differ here: Heart Songs 1909 against Benteen 1850.
  2. **Bar 24, the last note:**
     - A: `E4:6`. Heart Songs prints a quarter tied to an eighth, closing against the eighth pickup.
     - B: `d2`, which is `E4:8` after the move. Benteen's voice enters after a piano introduction, so its last bar is full.
     - This is a difference in a note's length, not in a rest, so the ruling that lets me append a rest doesn't apply. It is print layout, not melody.
- **Everything else agrees.** I checked this in a throwaway copy where both bars were made equal. That copy passed, and it is not delivered.
  - The unsure slurred-pair bars agree with the 1850 print: 1, 5 and 6 merged, and 9, 13, 14 and 21 split. This includes `C#5:1 C#5:1 B4:1 B4:1 G#4:4` in bar 14.
  - Bars 16 and 18, with rests, agree under the rest rule.
- **For the user to settle:** Zitro plays A (1909), and B is the first edition (1850). They differ in bar 23 (8-16-16-8-16-16 against 8-16-16-16-8-16) and in the last note (`E4:6` against a half note).
- Not used: the Commons MIDIs (`Camptown Races.mid`, `Camptown Races.MID`, `Camptown Races (1850), composed by Stephen Foster.mid`). I didn't need them, and I didn't check their licences.

## greensleeves: PASS

- **Score B:** https://en.wikisource.org/wiki/Page:Popular_Music_of_the_Olden_Time,_Volume_1.pdf/264
  - A LilyPond transcription of Chappell, vol. 1, p. 230, "Tune of Green Sleeves. Oldest copy." The page is proofread, level 3.
  - Wikisource text is CC BY-SA 4.0.
- **Same print as A.** This is someone else's typesetting of A's own page. The PASS confirms A's reading of the print. It is not agreement between two editions. It is the 19th-century "oldest copy" form, as the controller preferred.
- **Part taken:** the top note of each chord in the upper staff (the right hand), which is the melody.
  - In this markup the melody note is always written first in each chord, for example `<g e>4`, `<b d,>8.` and `<d' d,>4.`.
  - In relative mode a chord's first note is also the octave reference for the next note. So keeping only the first note of each chord leaves every octave unchanged.
  - Left out: the other chord notes, the bass staff, the lyrics, the slurs and `\sf`.
- **Result: PASS.** These bars all agree:
  - the flagged bars 4 and 12 (B3),
  - the bar 8 eighth rest,
  - the turns in bars 7 and 15,
  - the shortened last bar, `e4. e4`.
- **Other open scores I looked at but did not use:**
  - **Mutopia "Greensleeves (hymntune)":** public domain, `ftp/Traditional/greensleeves`. It is the "What Child Is This" form, and differs in bar 4 (`fis'4.` with no D#) and in bar 13 (C natural).
  - **fr.wikipedia and br.wikipedia:** ABC, "Alas my love", in G minor. It differs in bars 8, 15 and 16.
  - **hu.wikipedia:** LilyPond. It differs at least in bar 8.
  - **Mutopia guitar (CC BY-SA, "from memory") and accordion arrangements:** these are in 3/4.

## londonderry-air: PASS

- **Score B:** https://en.wikipedia.org/wiki/Londonderry_Air, section "Music score": "The melody appears thus in the first edition".
  - A LilyPond melody of Petrie 1855, CC BY-SA 4.0.
- **Same print as A.** The PASS confirms A's reading of Petrie. It is not a "Danny Boy" sheet.
- **Part taken:** the whole score. It is a single melody staff.
- **Result: PASS.**
  - **Bar 6:** B has `f8 es c b c d es f`. In nederlands, `b` is B natural, so B agrees with A's B natural (B3). Petrie's B natural in bar 6 is confirmed.
  - The unsure bars 13 (`g'4. f8 f es c es`, G5 at the top) and 15 agree.
  - The eighth rests after the long notes agree exactly.
  - The last bar, `es2~ es8`, agrees.

## funiculi-funicula: DIFF (trailing rest only)

- **Score B:** https://it.wikipedia.org/wiki/Funicul%C3%AC_funicul%C3%A0, section "Testo e melodia". LilyPond, CC BY-SA 4.0, Italian text.
  - The markup and the page name no source edition. A search for Ricordi, ©, copyright, "tratto" and "fonte" finds nothing about the score.
- **Part taken:**
  - `strofa` (`\voiceOne`): the solo verse with its "Coro" echoes, all on one line.
  - Then `ripresaA` (`\voiceOne`): the upper voice of the refrain in the treble clef, marked "La 1ª volta, da solo".
  - Left out: `ripresaB` (the lower voice), `ripresaC` (the bass clef), the lyrics, the markups, the dynamics and the accents.
  - The language is `deutsch`, so `b` is B flat.
- **One edit to the markup, with no pitch changed:** `ripresaA` is its own `\relative c''` block in the source. Joined into one line after the verse, which ends on F4, its first note `d4.` is written `d'4.`, so that it stays D5 as in the source.
- **Result: DIFF.** The only difference is a trailing rest.
  - In the last bar, A has `Eb4:2 R:2 R:2 R:4`, 10 sixteenths, shortened against the eighth pickup. B has `es4 r8 r4.`, 12 sixteenths. B's refrain is a separate `\score` with no pickup, so its last bar is full.
  - Tool: `A 63/10, B 63/12` (event 123).
  - The song would pass if B's final `r4.` could be trimmed to `r4`. That is the mirror of the ruling that lets me append a rest, and the ruling doesn't cover it, so the controller or user must decide.
  - A throwaway copy with that one trim passed, so there is no other difference.
- **Flagged bars, all agreeing:**
  - **Bars 20–21:** A natural. B has `a4 a8`, and LilyPond pitch names are absolute.
  - **Bar 40:** a tie. B has `d4.~ d4 r8`.
  - **Bar 48:** Eb5. B has `es'2.`, which confirms the amendment in `transcripts.md`.
  - **Bar 36:** the rests.
  - **Bars 37, 38, 41 and 42:** "Listen!". B has `d4. c8 r8 r`.
  - **Bars 4, 13, 22 and 31:** the ties over the bar line.
  - **All chorus echoes.** The Italian B and the English A have the same notes throughout.

## long-long-ago: NEEDS BLIND READING

- **No copyable open score exists.** I searched these places:
  - Wikipedia: the en, ja and vi articles have no `<score>`. de.wikipedia has no article under "Lang, lang ist's her", "Lang ist es her" or "Long, Long Ago".
  - en.wikisource: the only hit is *The Scottish Minstrel*, which has the words only.
  - de.wikisource, en.wikibooks and Commons: no MIDI, MusicXML or LilyPond.
  - Mutopia: nothing by Bayly.
- **The open scores that exist are image PDFs on IMSLP:** https://imslp.org/wiki/Long,_Long_Ago_(Bayly,_Thomas_Haynes)
  - **Horn, "For Voice and Piano":** typeset by Marco Gallo (www.cipoo.net), **CC BY-SA 4.0**, file `PMLP680744-LongLongAgo.pdf`. The text is altered ("Sing me the songs …").
  - **Saboohi, "For 2 Violins and Piano":** **CC BY-SA 3.0**, file `PMLP432298-long,_long_ago.pdf`.
- **Candidate pre-1931 scan for the blind reader**, a different edition from A's (*Heart Songs*, 1909):
  - **First choice:** T. H. Bayly, *Long, Long Ago! A Ballad*, Cramer, Addison & Beale, London, n.d. [1839], plate 2538, public domain. https://archive.org/details/BaylyLongLongAgo1839Sheetmusic. It is also on Commons.
  - **Second choice:** the Firth & Hall, New York edition: https://archive.org/details/sm_longlongago
- `score-b` stays `TODO`.
- **Legal flag for the user.** The IMSLP work page comments: "Unclear whether Bayly is the composer here or librettist only." It notes that Charles Edward Horn (1786–1849) arranged another Bayly song.
  - The README keeps a song with disputed authorship out. Every candidate died long before 1926, so this is the same kind of question as Santa Lucia.

## Legal fields

None of the five songs has a legal-field problem in the tool run. Long, Long Ago fails only on its `score-b TODO`. Each song's two composer URLs are on different sites: Wikipedia and IMSLP.

## Final tool output

The scratch library holds the four committed songs plus the five drafts as delivered. The command was `python3 -I -B scripts/make_music_data.py --library <scratch>/lib --out <scratch>/out`, run from the repo root.

```
Nothing written. Failed:
  camptown-races: score B differs from score A at event 77 (MIDI/sixteenths, B moved by +2), after joining ties and folding rests: A 68/2 66/1 66/1 64/6, B 68/1 66/2 66/1 64/8; fix the notes, or add a settled line saying why a person accepts the difference
  funiculi-funicula: score B differs from score A at event 123 (MIDI/sixteenths, B moved by +0), after joining ties and folding rests: A 63/10, B 63/12; fix the notes, or add a settled line saying why a person accepts the difference
  long-long-ago: score-b must be: url | language | relative <pitch> or absolute | LilyPond notes
Accepted difference: auld-lang-syne: score B differs from score A at event 1 (MIDI/sixteenths, B moved by +2), after joining ties and folding rests: A 62/2 67/3 67/1 67/2, B 62/4 67/6 67/2 67/4 (settled: Matthias 2026-11-07: no open score of the 2/4 version exists, so B (en.wikipedia, moved +2) is 4/4 at doubled values and also differs in bar 9 (A D5:2 B4:2 B4:2 G4:2, B halved D5:3 B4:1 B4:2 G4:2) and bar 10 (A A4:3 G4:1 A4:2 B4:2, B halved A4:3 G4:1 A4:2 E5:2); Zitro plays the old print's version (1897), and a second reading of A's scan confirmed A.)
Accepted difference: ihr-kinderlein-kommet: score B differs from score A at event 42 (MIDI/sixteenths, B moved by +2), after joining ties and folding rests: A 68/2 66/2 64/6, B 66/2 63/2 64/6 (settled: Matthias 2026-11-07: bars 15-16 ("Freude uns macht"), A (Schott 1900) has A4:4 G#4:2 F#4:2 / E4:4 R:2, B (de.wikipedia, moved +2) has A4:4 F#4:2 D#4:2 / E4:6; the two prints end differently, and it is not verified which ending is Schulz's and which is Eickhoff's; Zitro plays the old print's version, and a second reading of A's scan confirmed A.)
Accepted difference: oh-susanna: score B differs from score A at event 50 (MIDI/sixteenths, B moved by +0), after joining ties and folding rests: A 74/2 76/2 74/2 71/2, B 74/3 76/1 74/2 71/2 (settled: Matthias 2026-11-07: the Holt and Benteen 1848 editions differ in rhythm in bar 13 (A B4:2 D5:2 D5:2 E5:2, B B4:2 D5:2 D5:3 E5:1), bar 15 (A B4:2 B4:2 A4:2 A4:2, B B4:1 B4:3 A4:3 A4:1) and bar 22 (A D5:2 B4:2 G4:3 A4:1, B D5:2 B4:2 G4:2 A4:2); Zitro plays the old print's version (Holt), and a second reading of A's scan confirmed A.)
exit 1
```

A second run with the four committed songs plus greensleeves and londonderry-air only:

```
6 songs written to <scratch>/out2: 8 files, 9049 bytes
exit 0
```
