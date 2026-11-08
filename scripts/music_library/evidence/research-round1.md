# Task 6 research: five songs from pre-1931 scans

## Feasibility verdict: GO

All five songs were read from printed scans made before 1931, and every scan was usable.

- **Where the scans came from:** the Internet Archive did all the work. Its `page/nN_w2400.jpg` images are 300–350 dpi.
- **Sources that didn't work:**
  - IMSLP file downloads now sit behind a captcha ("Bot Check").
  - loc.gov is behind Cloudflare, and britannica.com returned 403.
  - liederlexikon.de was down (HTTP 503) all session.
  - None of these were needed.
- **How the scans read:**
  - The 19th-century engravings read cleanly once a single staff is cropped and zoomed.
  - Pitch is the error-prone part. Staves on old sheet music are skewed, and one staff on the Holt 1848 sheet rises about 30 px across the page.
  - I therefore measured noteheads against the five local staff lines with a small pixel helper. Where it failed, the note was read from a zoomed image and is flagged, written in scratch and not part of the repo: `tools/heads2.py` finds filled heads, `tools/heads.py` finds staff lines. I compared that with the zoomed image. Durations, dots, beams and rests were read by eye.
- **Effort:** about one bar per song needed a judgment call. Each is flagged below.
- **Self-test:** a scratch copy of the five drafts passes `scripts/make_music_data.py`. For that run only, score B was a placeholder copy of my own notes, written in scratch and never in this directory. That shows the names, aliases, evidence fields, bar lengths and range are all accepted. In the real drafts the only open item is `score-b TODO`.
- **Transposition:** none was needed. Every song sits inside A3–A5 (MIDI 57–81) as printed.

The transcriptions were logged to a scratch file before any comparison: `<scratchpad>/transcripts.txt`.

### Independence: what I looked at

- I never opened a Wikipedia score, `<score>` markup, rendered score image, LilyPond, ABC or MIDI file. I read Wikipedia only through the `prop=extracts` plain-text API.
- I read the IMSLP work pages' wikitext for metadata only: composer dates, publication year and file list. I opened no IMSLP score.
- **Two disclosures, both other pre-1931 prints and neither one a score-B candidate:**
  1. **Oh! Susanna:** before choosing Holt 1848, I opened Commons `Oh!_Susanna_2.jpg` (Duke n1329, the Benteen Baltimore edition credited to "Wells"). That page shows the first verse. I discarded it because it credits the wrong author, and I transcribed from Holt only.
  2. **Der Mond ist aufgegangen:** I transcribed from Erk Bd. II first and logged it. Only then did I open Erk Bd. I, the same publisher in a different engraving, to settle one bar (see below).

## Songs drafted

### 1. oh-susanna: Oh! Susanna (en)

- **Scan:** C. Holt Jr., New York 1848, "Oh! Susanna, sung by G. N. Christy of the Christy Minstrels". It is No. 8 of "Music of the Original Christy Minstrels". Copy: Connecticut College, Greer Music Library, Historic Sheet Music Collection 850, http://digitalcommons.conncoll.edu/sheetmusic/850.
  - Read from https://archive.org/details/sm_ohsusanna. The music pages are https://archive.org/details/sm_ohsusanna/page/n3/, `/page/n4/` and `/page/n5/` (PDF pp. 4–6). The cover is `/page/n1/` (PDF p. 2) and gives the 1848 copyright line.
- **Read:** 2/4 in G, pickup of two sixteenths. Verse (16 bars) and chorus (8 bars), with the chorus taken from the "1st Voice" staff.
- **Sure:** all pitches except the three spots below were measured by the helper and agree with the eye. Rhythms are clear, with dotted-eighth and sixteenth figures throughout.
- **Eye-read, helper inconclusive:** the staff is strongly skewed here, so the helper's line fit failed in three places. Score B should check these first:
  - Bar 11 ("weather it was"), 4th note: read as G4. The helper gave A4, then E4, both with a bad line fit. Measured against the lines found at x≈600, the head sits between F4 and G4, nearest G4.
  - Bar 12, the "The" pickup sixteenths: read as G4 A4. The helper gave D4 for the first one, with a bad line fit.
  - Bar 20, the "I've" pickup sixteenths: read as G4 A4 from a 400% zoom. The helper gave F4 at about 1.5 steps.
- **Flagged:**
  - Bar 14 ("death;") has a fermata over G4. It is ignored.
  - Bar 16 ("cry."): G4 quarter, then a rest drawn in the old mirrored-eighth shape. I read it as a quarter rest because the bar needs 4 more sixteenths.
  - Last bar: the print has G4 quarter, rest and a repeat sign. It is written `G4:4 R:2` so the song closes against the 2-sixteenth pickup.
  - Holt's verse text differs from later editions ("Susanna, dont you cry" ends the verse). Text doesn't matter here. The melody matches the verse bars of the 1st stanza.
- **Edition note:** Holt's sheet does not name Foster on its pages. The library catalogue credits Stephen Collins Foster.
- **Evidence:**
  - Composer: Stephen Foster, 1826–1864.
    - https://en.wikipedia.org/wiki/Stephen_Foster ("July 4, 1826 – January 13, 1864")
    - https://imslp.org/wiki/Category:Foster,_Stephen (born 1826-07-04, died 1864-01-13)
  - Published 1848:
    - https://en.wikipedia.org/wiki/Oh!_Susanna ("first published in 1848")
    - https://imslp.org/wiki/Oh!_Susanna_(Foster,_Stephen) ("Year of First Publication=1848")
- **Tempo** 100 (marked Allegretto). **Footer** "Holt 1848".

### 2. der-mond-ist-aufgegangen: Der Mond ist aufgegangen (de)

- **Scans:** both are Erk's Deutscher Liederschatz, C. F. Peters, Leipzig. The Internet Archive dates both copies 1890, and Erk died in 1883. Both scans are from the University of Toronto, and the heading reads "Abendlied … J. A. P. Schulz (1790.)".
  - Transcribed from Bd. II, Nr. 33 (233), p. 34, plate 7287: https://archive.org/details/deutscherlieders00erkl_1/page/n41/
  - Bar 6 settled from Bd. I, Nr. 22, p. 234, plate 7390: https://archive.org/details/deutscherlieders00erkl_0/page/n243/
  - `source-a` cites Bd. I, the print in which every symbol is present. All other notes in it are visually identical to Bd. II.
- **Read:** 4/4 in F, pickup of a quarter, 12 bars.
- **Sure:** all pitches, measured. This is a clean modern engraving.
- **Flagged:**
  - Bar 6 ("klar; der"): Bd. II prints only a half G4 and a quarter F4, 3 beats, with a gap and no rest. Bd. I prints a half G4, a quarter rest and a quarter F4, so I read `G4:8 R:4 F4:4`.
  - The final fermata is ignored.
- **Evidence:**
  - Composer: Johann Abraham Peter Schulz, 1747–1800.
    - https://de.wikipedia.org/wiki/Johann_Abraham_Peter_Schulz (31 March 1747 – 10 June 1800)
    - https://imslp.org/wiki/Category:Schulz,_Johann_Abraham_Peter (1747-3-31 to 1800-6-10)
    - https://en.wikipedia.org/wiki/Johann_Abraham_Peter_Schulz agrees.
  - Published 1790, in Lieder im Volkston, Teil 3:
    - https://de.wikipedia.org/wiki/Abendlied_(Matthias_Claudius) ("Sammlung Lieder im Volkston … aus dem Jahr 1790")
    - https://imslp.org/wiki/Lieder_im_Volkston_(Schulz,_Johann_Abraham_Peter) (Part 3: Heinrich August Rottmann, Berlin 1790, Abendlied = Part 3 No. 51)
- **Tempo** 72 (marked "Sehr gemässigt"). **Footer** "Erk, Peters c. 1890".

### 3. weisst-du-wie-viel-sternlein: Weißt du, wie viel Sternlein stehen (de)

- **Scan:** Unser Liederbuch: die beliebtesten Kinderlieder, Bd. 1, B. Schott's Söhne, Mainz. The book prints "Copyright 1900 by B. Schott's Söhne" on p. 3. The copy is from Boston Public Library.
  - Page 39 ("Gott weiss Alles"): https://archive.org/details/unserliederbuchd01merc/page/n42/
- **Read:** 3/4 in F, pickup of two eighths.
- **Repeat:** the first line carries a repeat sign (stanza lines 1–2 share the music). I wrote it out, so the draft is one complete verse of 16 bars plus the pickup. Score B will need `\repeat unfold`/`volta` handled to match.
- **Sure:** every pitch, measured, including the 2-eighth slurred figures D5-C5 and C5-Bb4.
- **Worth a second look when score B arrives:** bar 3 reads C5 Bb4 Bb4 **C5** Bb4. The 4th note is clearly in the C5 space, not a sequence copy of bar 2's A4.
- **Evidence (traditional):**
  - https://de.wikipedia.org/wiki/Wei%C3%9Ft_du,_wie_viel_Sternlein_stehen ("Volksweise, deren heutige Fassung seit 1818 belegt ist"; borrowed from "So viel Stern' am Himmel stehen" and the soldiers' song "O du Deutschland, ich muss marschieren")
  - https://en.wikipedia.org/wiki/Wei%C3%9Ft_du,_wie_viel_Sternlein_stehen ("The melody is recorded back to 1818")
  - https://hymnary.org/tune/weisst_du_wie_viel_sternlein_german ("Traditional German Melody")
  - Named prints: the 1818 "Deutsche Lieder für Jung und Alt", per de.wikipedia, and the 1900 Schott print itself.
- **Tempo** 92 (marked "Ruhig"). **Footer** "Unser Liederbuch, Schott 1900".

### 4. ihr-kinderlein-kommet: Ihr Kinderlein, kommet (de), replacing "Kling, Glöckchen"

- **Scan:** the same book as song 3, Unser Liederbuch Bd. 1, Schott, Mainz 1900.
  - Page 24 ("Weihnachtslied"): https://archive.org/details/unserliederbuchd01merc/page/n27/
- **Read:** 2/4 in E major (4 sharps), pickup of one eighth, 16 bars.
- **Sure:** every pitch, measured. The eighth rests in bars 4, 8 and 12 are clear.
- **Last bar:** the print has E4 quarter and an eighth rest, which is `E4:4 R:2` and fits the pickup.
- **Authorship:** two closing versions exist. Schulz wrote the tune in 1794 (to "Wie reizend, wie wonnig"). Eickhoff fitted it to Schmid's text in 1832 and "veränderte die Schlusswendung"; that is the common version. I can't tell from the print alone whose ending it uses, so both are listed as `composer`, and both died long before 1926. Score B must use the same ending, or a `settled` line is needed.
- **Evidence:**
  - Schulz: as in song 2, de.wikipedia plus IMSLP.
    - https://en.wikipedia.org/wiki/Johann_Abraham_Peter_Schulz also names him as composer of "Ihr Kinderlein kommet".
  - Eickhoff, 1807–1886:
    - https://de.wikipedia.org/wiki/Friedrich_Eickhoff ("* 1807 in Soest; † 1886 in Gütersloh")
    - https://www.deutsche-biographie.de/pnd141108711.html ("Lebensdaten 1807 – 1886, Geburtsort Soest, Sterbeort Gütersloh"; GND 141108711)
    - The GND record agrees: https://d-nb.info/gnd/141108711, dateOfBirth 1807, dateOfDeath 1886.
    - https://en.wikipedia.org/wiki/Ihr_Kinderlein,_kommet also gives "Friedrich Eickhoff (1807–1886)".
  - Published 1795, as `published 1795`: https://de.wikipedia.org/wiki/Ihr_Kinderlein,_kommet ("erschien erstmals im Taschenbuch zum geselligen Vergnügen auf das Jahr 1795").
- **Tempo** 100 (marked "Freudig"). **Footer** "Unser Liederbuch, Schott 1900".

### 5. auld-lang-syne: Auld Lang Syne (en)

- **Scan:** The Scottish Students' Song Book, Bayley & Ferguson, London 1897, pp. 318–319, "Auld Lang Syne, arr. by Sir Herbert Oakeley". The copy is from Boston Public Library.
  - p. 318: https://archive.org/details/scottishstudents00scot_0/page/n325/
  - p. 319: https://archive.org/details/scottishstudents00scot_0/page/n326/
- **Read:** 2/4 in G, pickup of one eighth. Verse 1 (8 bars, solo staff), then the chorus (8 bars, top staff). The chorus uses the "4 first V." ending (G4 dotted quarter), which fits the pickup.
- **Sure:** every pitch, measured. The dotted-eighth and sixteenth figures are clear, and there are no Scotch snaps or triplets.
- **Flagged:**
  - Verse bar 4 ("min'? Should"): next to the main-size E5 eighth there are small cue notes (G5 dotted sixteenth and E5 thirty-second) for verse 2's extra syllable ("But we've"). They are left out. The main-size notes are `E5:6 E5:2`.
  - **Chorus bar 12 ("syne; We'll"): least sure.** The first-tenor staff is in divisi at "We'll": E5 with stem down, G5 with stem up. I chose E5 to match the solo verse line at the same place. If score B has G5, this one note needs settling. It is not a misreading of the scan.
  - Chorus bar 9 ("auld lang"): plain eighths `D5 B4 B4 G4`, with no dots. I checked this at 250%.
- **Evidence (traditional):**
  - https://en.wikipedia.org/wiki/Auld_Lang_Syne ("In 1799 it was set to a traditional pentatonic tune"; "The first documented use of the melody commonly used today was in 1799, in the second volume of George Thomson's Select Songs of Scotland")
  - https://hymnary.org/tune/auld_lang_syne (source "Scotch Melody", composer "Anonymous", Public Domain)
  - Named prints: Thomson 1799 and the 1897 song book.
  - en.wikipedia also says William Shield (1748–1829) "seems to quote" the tune in Rosina (1782), and that "there is some doubt" whether Burns intended this melody. Neither names an author, and Shield died long before 1926 anyway, so I did not treat it as a disputed authorship. The reviewer should judge that.
- **Aliases:** "nehmt abschied brueder" is the German song title sung to this tune.
- **Tempo** 76. **Footer** "Scottish Students' Song Book 1897".

## Dropped

- **Kling, Glöckchen: the melody's authorship is disputed.**
  - https://de.wikipedia.org/wiki/Kling,_Gl%C3%B6ckchen,_klingelingeling says the composer of today's melody is unknown. It calls the Widmann attribution "offenkundig auf einer Verwechslung" and gives an anonymous print of 1862 (Oberhoffer). The first 1854 print had a different melody, by Wilhelm Speyer.
  - https://en.wikipedia.org/wiki/Kling,_Gl%C3%B6ckchen says it is a traditional folk tune, or set by Benedikt Widmann (1820–1910) in 1884.
  - Musicanet and IMSLP list Widmann.
  - Every named candidate died before 1926, but the sources disagree, and the rule says disputed authorship stays out. I did not look at any score of it.
- **Alle Jahre wieder: considered as the replacement and passed over.** The book's setting (Unser Liederbuch p. 3) names no composer. de.wikipedia lists three melodies, by Silcher, Anschütz and Rinck, and I can't tell from the print alone which one it is without comparing against another score.

## Score B (Task 6, second half)

**Verdict: none of the five songs passes.** Every song has an open, independent score B. In every case, B differs from A at one or more places. I re-opened A's scan for each of those places, and in every case the scan unambiguously shows A's reading. So **no correction was made to score A**. No `settled` line was written: only the user may settle a difference. Each difference is listed below as bar, A, B. Bar numbers count A's bars, with the pickup as bar 0.

### How score B was searched

- **Wikipedia.** The `<score>` blocks of the de, en and fr articles, fetched as wikitext through the API.
- **Wiki-wide search.** CirrusSearch `insource:/\<score/` plus the title, run on these wikis: en/de Wikipedia, nl/fr Wikipedia, Commons, de/en Wikisource, and de/en Wikibooks. The rate-limited queries were re-run until they answered.
- **Commons.** The categories of all five songs, and a file search.
- **Mutopia.** A title and composer search.
- **Not reachable:** CPDL, which answered behind a Cloudflare challenge (HTTP 403), and IMSLP, which is behind a captcha.
- **How the B lines were checked:**
  - Every `score-b` line was compared token by token against the fetched LilyPond, with slurs, beams and marks stripped.
  - The only intended changes are these: Sternlein's two `\repeat volta` blocks without alternatives are written `\repeat unfold 2`, as the README asks; Mond's `\tempo` mark inside the alternative is dropped; and Susanna's chorus repeat is played once, as in A (see that song).
- **Where the downloads are:** in the scratchpad, `scoreb_dl/`, `mutopia/`, `midi_dl/`, `auld_dl/` and `scans_b/`, with the crops in `scans_b/z_*.png`.
- **The comparison tool:** `scripts/make_music_data.py`, run on a scratch copy. A small scratch helper (`tools/sdiff.py`) uses the tool's own `parse_notes`/`read_score`/`join` to list *every* difference, where the tool lists only the first.

### 1. oh-susanna: FAIL (5 rhythm differences, a variant edition)

- **Score B:**
  - https://en.wikisource.org/wiki/Page:Oh!_Susanna_(IA_OhSusanna_201611).pdf/2, with the notes continuing on `/3` and `/4`.
  - It is a proofread LilyPond transcription, by Wikisource user Edderiofer, of the F. D. Benteen, Baltimore print "Written by WELLS", as sung by the Ethiopian Serenaders (title page `/1`). That is a different 1848 edition from A's Holt, New York.
  - **Licence:** Wikisource text is CC BY-SA 4.0, and the 1848 print is public domain.
  - Only the "Air" voice was taken, from m. 9 (the "I" pickup) to m. 33.
- **The repeat:** the chorus carries `.|: … :|.` in B, and Holt has a repeat sign too. A plays the chorus once, so B was copied once as well, ending `g4 r8`.
- **Independence caveat (see disclosure 1 above):**
  - A's transcriber had opened Commons `Oh!_Susanna_2.jpg`, the Benteen first page, before choosing Holt.
  - So B's agreement on the verse is not fully independent.
  - The chorus (pages 3–4 of Benteen) was not seen.
- **Pitches:** all of them agree, with no transposition.
  - That includes A's three flagged spots: bar 11, 4th note G4; bar 12, the pickup G4 A4; and bar 20, the "I've" pickup G4 A4. All three are confirmed by B.
  - Bars 11 and 12 fall under the caveat above.
- **Differences:** all rhythm. I checked each on the Holt scan (`sm_ohsusanna`, `page/nN_w2400.jpg`), and each glyph is clear, so A stays.

| Bar | Words | A (Holt) | B (Benteen/Wikisource) | Holt scan evidence |
|---|---|---|---|---|
| 8 | "see, It" | G4:6 G4:1 A4:1 | G4:4 R:2 G4:1 A4:1 | n3, crop x1000–1480 y2680–2940: quarter G with an augmentation dot, no rest, then two beamed sixteenths |
| 13 | "sun so hot I" | B4:2 D5:2 D5:2 E5:2 | B4:2 D5:2 D5:3 E5:1 | n4, crop x1600–2380 y360–680: four single-flag eighths, no dot |
| 15 | "an-na, dont you" | B4:2 B4:2 A4:2 A4:2 | B4:1 B4:3 A4:3 A4:1 | n4, crop x1000–1780 y1120–1380: four single-flag eighths, no dots |
| 20 | "me, I've" (chorus) | A4:6 G4:1 A4:1 | A4:4 R:2 G4:1 A4:1 | n4, crop x1840–2380 y1880–2140: quarter A with a dot, no rest, then two beamed sixteenths |
| 22 | "ba-ma, wid my" | D5:2 B4:2 G4:3 A4:1 | D5:2 B4:2 G4:2 A4:2 | n5, crop x980–1480 y370–660: dotted G eighth, then a double-flag A sixteenth |

- **Other open scores looked at, none matching Holt:**
  - fr.wikipedia "Oh! Susanna (chanson)" (CC BY-SA). It is another variant: the verse is like Benteen's but with its own bars 13/15, and it has `a4 r8` at bar 12.
  - de/en Wikibooks "Liederbuch/ Oh Susanna" / "Songbook/Oh, Susanna" (CC BY-SA). These are 4/4 at doubled values, so the lengths can never match.
  - Commons/IA `OhSusannaOriginal1848SheetMusic`. That is the Holt edition itself, so it is not independent.
  - IA `imslp-susanna-foster-stephen`. That is a modern edition by R. A. Hudson whose licence I can't verify, because IMSLP sits behind a captcha. Not used.
- **For the user to decide:** whether the Holt-vs-Benteen rhythm differences may be settled.

### 2. der-mond-ist-aufgegangen: FAIL (1 difference, bar 6)

- **Score B:** https://de.wikipedia.org/wiki/Abendlied_(Matthias_Claudius), `sopranoVoice`. CC BY-SA 4.0.
  - en.wikipedia "Der_Mond_ist_aufgegangen" carries the identical code, so it is one source, not two.
- **Confirmed by two more independent open sources, both agreeing with B and not with A:**
  - Mutopia piece 127, https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=127 (`mond.ly` by G. Roesler, Public Domain). It has `g2. f4` in bar 6, and the rest is identical to A.
  - Commons `File:Der_Mond_ist_aufgegangen.mid` (Peter Gerloff, EG 482 setting, CC0). This is MIDI and is used only as support. The melody G4 in bar 6 is held 11 of 12 sixteenths, then F4 follows, so a dotted half and no rest.
- **The difference:**

| Bar | Words | A | B (all three) |
|---|---|---|---|
| 6 | "klar; der" | G4:8 R:4 F4:4 | G4:12 F4:4 |

- **A's scan evidence:** Erk Bd. I, https://archive.org/details/deutscherlieders00erkl_0/page/n243/, `n243_w2400.jpg` (2800×4157), crop x900–1560 y1680–1960 (`scans_b/z_mond_bar6.png`). It shows a half-note G (open head), a clear quarter-rest glyph in the middle of the bar, then a quarter F.
  - So A stays. This is A's flagged bar, and the scan is unambiguous.
  - The difference is an edition variant: Erk/Peters prints a rest where the hymnal tradition holds the note.
- No open score with the rest was found. **For the user:** settle, or drop the song.

### 3. weisst-du-wie-viel-sternlein: FAIL (3 differences, phrase ends)

- **Score B:** https://de.wikipedia.org/wiki/Wei%C3%9Ft_du,_wie_viel_Sternlein_stehen, `sopranoVoice`. CC BY-SA 4.0.
  - en.wikipedia has the identical code, so it is one source.
- **Provenance flag:** the LilyPond carries the comment `% from https://www.liederprojekt.org/lied27784-…`. That site is "© CARUS-VERLAG 2026", and no open licence was found on it. The melody itself is public domain, and the Wikipedia page is CC BY-SA, but the user should know that this B is a re-encoding of a Carus edition.
- **What B confirms:** every pitch agrees, including A's "second look" note, bar 3 4th note C5 (B has `c8 bes` there).
  - The Commons CC0 MIDI (Gerloff, EG 511) also has C5 there, and D5:4 C5:4 in bar 14.
- **Differences:**

| Bar | Words | A | B |
|---|---|---|---|
| 4 | "-zelt?" | A4:6 R:2 | A4:8 |
| 8 | "Welt?" (written-out repeat of bar 4) | A4:6 R:2 | A4:8 |
| 16 | "Zahl." (last) | A4:6 R:2 | A4:8 |

- **A's scan evidence:** Unser Liederbuch p. 39, `unserliederbuchd01merc/page/n42_w2400.jpg` (3346×2737).
  - Bar 4 is at crop x2250–2580 y880–1100: a quarter A with an augmentation dot, then an eighth-rest glyph, then the repeat dots.
  - Bar 16 is at crop x2250–2580 y1750–2000: the same dotted quarter plus eighth rest.
  - Bar 8 is the same printed bar as bar 4, played again under the repeat sign.
  - A stays.
- **Support for A, not used as B:** the Commons MIDI `File:Weißt_du,_wie_viel_Sternlein_stehen.mid` (CC0) has the same figure as A at bars 4 and 8: A held about 6 sixteenths, then 2 sixteenths of silence. But its final A is held about 8, so it is ambiguous at bar 16. It is a MIDI rendering, not a notated score, so it is not used as B.
- The two de.wikibooks hits ("Gitarre: Folkdiplom", "Gitarre: G7 Akkord") contain no Sternlein melody.
- **For the user:** settle the three dotted-quarter-plus-rest endings, or drop the song.

### 4. ihr-kinderlein-kommet: FAIL (the closing variant A predicted)

- **Score B:** https://de.wikipedia.org/wiki/Ihr_Kinderlein,_kommet (CC BY-SA 4.0), in D major. B is moved by +2 semitones.
  - en.wikipedia has identical code.
  - de.wikibooks "Liederbuch/ Ihr Kinderlein Kommet" (CC BY-SA, in G major) has the same closing.
- **Differences:**

| Bar | Words | A | B (moved +2) |
|---|---|---|---|
| 15 | "Freude uns" | A4:4 G#4:2 F#4:2 | A4:4 F#4:2 D#4:2 |
| 16 | "macht." | E4:4 R:2 | E4:6 |

- **A's scan evidence:** p. 24, `unserliederbuchd01merc/page/n27_w2400.jpg`, crop x2630–3010 y1140–1260.
  - The staff lines are at y 1157 / 1177.5 / 1199 / 1220.5 / 1241, about 21 px apart.
  - The heads measure 3.01, 2.02, 1.04 and 0.03 half-spaces above the bottom line. That is A4, G#4, F#4 (the key has 4 sharps), then E4 on the bottom line, followed by an eighth-rest glyph. The view crop is `scans_b/z_kind_end.png`.
  - A stays.
- **Which ending is which:**
  - Every open notated score found has B's leaping ending.
  - Commons labels two audio files, a "Schluss nach Eickhoff" version (EG 43) and a "Schluss nach JAP Schulz" version (Gotteslob 248, `File:Ihr_Kinderlein_kommet_JAP_Schulz1794.ogg`, GFDL, migrated to CC BY-SA).
  - Which of the two A matches is **not verified**. I compared neither audio file with A, and no notated score of the Schulz-ending version was found.
- **Consequence for the composer lines:** only if A turns out to be the Schulz ending might Eickhoff's `composer` line be unnecessary. That is the user's call.
- **For the user:** settle, or drop the song.

### 5. auld-lang-syne: FAIL (no open 2/4 score of A's version)

- **Score B:** https://en.wikipedia.org/wiki/Auld_Lang_Syne (CC BY-SA 4.0). It is in F, in 4/4, at doubled note values.
- **The tool's verdict:** it fails at event 1, because every length is double A's. Rescaling is not allowed.
- **Comparison at half values,** for information only: the pitches agree with A, moved +2, except at two places.

| Bar | Words | A | en.wikipedia (moved +2, values halved) |
|---|---|---|---|
| all | | 2/4, eighth beat | 4/4, quarter beat (every length ×2) |
| 9 | "auld lang" | D5:2 B4:2 B4:2 G4:2 | D5:3 B4:1 B4:2 G4:2 |
| 10 | "syne, my dear, For" | A4:3 G4:1 A4:2 **B4:2** | A4:3 G4:1 A4:2 **E5:2** |

- **A's scan evidence:** Scottish Students' Song Book p. 319, `scottishstudents00scot_0/page/n326_w2400.jpg`, crop x300–1320 y1080–1330, 1st-tenor staff.
  - Bar 9 is two beamed pairs of plain eighths, D-B and B-G, with no dots.
  - Bar 10 is a dotted-eighth A, a sixteenth G, an eighth A, and an eighth on the middle line (B4) at "For".
  - A stays.
  - A's flagged bar 12 ("We'll", E5) is confirmed by en.wikipedia (`d4` there, which is E5 once moved).
- **The other open scores, none of them A's version:**
  - de.wikipedia Auld_Lang_Syne (CC BY-SA). It is in 2/4, but it is the Scotch-snap version: `f16 f8.`, `g8. a16` in bar 2, G5 at "Should" and at "We'll". There are 8 or more differences.
  - en.wikibooks "Songbook/Auld Lang Syne" (CC BY-SA). One score is 4/4. The other is Beethoven's 2/4 setting, with snaps and rests.
  - Wikisource "The Riverside song book" p. 88. It is 4/4 and set to different words.
  - Commons "37 Auld Lang Syne.png" (PD-self). It is 4/4 and has the high note at bar 10 "For".
  - Commons "Auld Lang Syne.jpg" (CC BY-SA). It is 3/4.
  - Commons Thomson/Kozeluch 1799 (PD scan). It is an ornate first print, not A's version, and reading it would be a fresh transcription.
  - Mutopia has only a Horetzky guitar arrangement.
- **For the user:** settle (meter plus bars 9 and 10), or drop the song.

### Corrections to score A

None. Each A/B difference was re-checked on A's own scan, and in every case the scan clearly shows A's symbol. A was never changed to match B.

### Final tool run

No song passes, so nothing was written. Command, run from `<repo>` on a scratch copy of the five updated drafts:

```
python3 -I -B scripts/make_music_data.py --library <scratchpad>/lib_final --out <scratchpad>/out_final
Nothing written. Failed:
  auld-lang-syne: score B differs from score A at event 1 (MIDI/sixteenths, B moved by +2): A 62/2 67/3 67/1 67/2, B 62/4 67/6 67/2 67/4; fix the notes, or add a settled line saying why a person accepts the difference
  der-mond-ist-aufgegangen: score B differs from score A at event 20 (MIDI/sixteenths, B moved by +0): A 67/8 R/4 65/4 67/4, B 67/12 65/4 67/4 65/4; fix the notes, or add a settled line saying why a person accepts the difference
  ihr-kinderlein-kommet: score B differs from score A at event 45 (MIDI/sixteenths, B moved by +2): A 68/2 66/2 64/4, B 66/2 63/2 64/6; fix the notes, or add a settled line saying why a person accepts the difference
  oh-susanna: score B differs from score A at event 30 (MIDI/sixteenths, B moved by +0): A 67/6 67/1 69/1 71/2, B 67/4 R/2 67/1 69/1; fix the notes, or add a settled line saying why a person accepts the difference
  weisst-du-wie-viel-sternlein: score B differs from score A at event 17 (MIDI/sixteenths, B moved by +0): A 69/6 R/2 65/2 67/2, B 69/8 65/2 67/2 69/4; fix the notes, or add a settled line saying why a person accepts the difference
(exit 1)
```

**Pattern worth the user's attention:** 12 of the 14 listed differences are rhythm or meter, not pitch, and most of them fall at phrase ends. The cases are a held note versus a note plus rest (Mond 6, Sternlein 4/8/16, Kinderlein 16, Susanna 8/20), dotted versus even pairs, and a meter written at doubled values. Only two are pitch differences: Kinderlein bar 15 and Auld bar 10. Under the current rule, a rest-versus-held-note engraving difference is enough to reject a song. If that is stricter than intended, the user may want to decide whether such differences need a `settled` line per song or a rule change.
