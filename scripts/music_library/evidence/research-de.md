# Task 9 round 2, German children's songs: score A research

**Feasibility: GO.** Five songs are drafted from scans of prints made before 1931. Two of the requested songs were dropped and replaced (see "Dropped"). All five drafts pass `scripts/make_music_data.py` in a scratch self-test. That run used a placeholder score B (our own notes), kept only in the scratchpad, never here. The drafts were written with `score-b TODO`; score B has since been filled in (see "Score B" at the end). A yes/no script found a `<score>` block in the de.wikipedia article of all five songs, so each has an open score B candidate. I did not look at any of them.

## How the work was done

- **Scans:** all come from the Internet Archive. I downloaded `page/nN_w2400.jpg` into the scratchpad (`r2de/scans/`).
- **Reading pitches:** I measured noteheads against the five local staff lines with last round's helper (`tools/heads2.py`). I then checked every bar at 130–250 % zoom. Durations, rests and dots were read by eye.
- **Log:** every transcription was written to `transcripts.md` in this folder before anything else about that song was opened.
- **Independence:**
  - I opened no Wikipedia score, `<score>` markup, rendered score image, LilyPond, ABC or MIDI.
  - Wikipedia was read only through the plain-text extracts API.
  - IMSLP was read for metadata only: composer dates and one work page's edition line.
  - **Disclosure (after logging):** once every transcription was logged, I opened two other pre-1931 prints to check the legal evidence. Neither changed score A.
    - Hoffmann/Richter 1842, the Vogelhochzeit page.
    - Erk Bd. I p. 58, "Das Steckenpferd".
    - Both are public-domain scans. They are also possible score B candidates, but neither is an independent check on my reading now that I have seen them.
  - **Disclosure:** while choosing sources I read the OCR text (words only, no image) of a second pre-1931 copy of "Muss i denn": Unser Liederbuch Bd. 2, pp. 25–26, Schott 1902. I saw none of its notes.
- **Transposition:** none. Every song lies between D4 and E5 as printed.
- **Triplets or thirty-seconds:** none in any of the five.

## Songs drafted

### 1. muss-i-denn: Muss i denn zum Städtele hinaus

- **Scan:** Erk's Deutscher Liederschatz Bd. I, Nr. 79 "Abschied", p. 82, C. F. Peters, Leipzig. The plate is 8759, and the Internet Archive dates the copy 1890.
  - Page: https://archive.org/details/deutscherlieders00erkl_0/page/n91/ (PDF leaf n91).
  - It is the same volume as last round's "Der Mond ist aufgegangen".
  - The heading reads "Nach einem älteren Volksliede von Heinr. Wagner (pseud. Wergan) (1825)" and "Volksweise (1825)".
- **Read:** 2/4 in E major (4 sharps), with a pickup of two sixteenths. 18 bars in all, plus the pickup.
- **Form: FLAGGED.**
  - The first part, the pickup plus bars 1–5, carries two text lines ("Muss i denn…" / "Wenn i komm…") and ends with a repeat sign.
  - The second part ("Kann i gleich…") also ends with a repeat sign.
  - The printed text of stanzas 2 and 3 shows the form A A B, with B sung once per stanza. So the draft writes A twice and B once. That is also the form usually sung.
  - The second A is note-for-note the same as the first. The print uses double stems where the two text lines split a note, for example "zum" (one syllable on two sixteenths) against "wenn i". Only the underlay differs.
  - Score B may write this with `\repeat volta`, or as A B only. If B differs only in the repeat, that is a matter of form, not of notes.
- **Sure:** every pitch, measured.
  - The two-sixteenth figures are clear: bar 1 "zum" A4–C#5, bar 3 "und" G#4–B4, bar 10 "i" C#5 and "wenn i" B4–E5, bar 12 "an" D#5–C#5.
  - The last bar is G#4 quarter plus an eighth rest, `G#4:4 R:2`. That fills 6 sixteenths and closes against the pickup.
- **Evidence (traditional):**
  - https://de.wikipedia.org/wiki/Muss_i_denn,_muss_i_denn_zum_St%C3%A4dtele_hinaus says Silcher took over an "altwürttembergische Melodie". The song first appeared in Silcher's Volkslieder für vier Männerstimmen, Heft 2, op. 8 Nr. 12, in 1827. The original text is lost.
  - https://imslp.org/wiki/Muss_i_denn_(Folk_Songs,_German) lists it as a German folk song, edition Bantock, 100 Folksongs of All Nations No. 29, Ditson, Boston 1911. I read the metadata only.
  - The Erk print itself (c. 1890) gives "Volksweise".
  - Silcher (1789–1860) is named as the adapter. He died long before 1926, so the song is safe even if he counts as a co-author.
- **Question for the user (English title):** the only English title is "Wooden Heart" (Elvis, 1960). That is a new arrangement with new English words, credited to Wise, Weisman, Twomey and Kaempfert. I left it out of `en` and the aliases, so `en` is the German title. Add it only if you want "Play Wooden Heart" to find this tune.
- **Tempo** 92 (marked Moderato). **Footer** "Erk Liederschatz I, Peters c. 1890".

### 2. die-vogelhochzeit: Die Vogelhochzeit ("Ein Vogel wollte Hochzeit machen")

- **Scan:** Erk, Deutscher Liederschatz: die schönsten Weisen der alten Sammlung Ludwig Erks, neu bearbeitet und durch hundert Lieder vermehrt. It is Edition Peters Nr. 395, plate 10613. The song is Nr. 46 "Vogelhochzeit" ("Neueres Volkslied", "Volksweise"), p. 62.
  - Page: https://archive.org/details/deutscherlieders00erkl/page/n79/ (PDF leaf n79).
  - The copy is from Boston Public Library.
- **Print date (score A only; flagged):**
  - The library catalogue gives "[192-?]", and the call number written on the cover is "M1734 .E658 1920". No printed year was found on the title pages.
  - That puts it in the 1920s, before 1931, but the date is the library's estimate.
  - The volume's line "Das Ergebnis der Neu-Revision … sind geschütztes Eigentum des Verlages" covers the arrangement only. The melody is a Volksweise.
  - The melody itself is in print in 1842 (see evidence), so its legal status does not depend on this date. Score A must still be a print from before 1931. If you doubt the 1920s date, the 1842 page could serve as score A instead, but its variants (see evidence) would change the notes.
- **Read:** 2/4 in G major, with a pickup of one eighth. 8 bars plus the pickup: verse 1 and the "Fidiralala" refrain.
- **Flagged, bar 4 ("Wal-de."):** A4–D5 beamed on "Wal", then D5 as an eighth with a **fermata**, which is ignored, then D4 D4 sixteenths ("Fi-di").
- **Sure:** every pitch, measured. It is a clean modern engraving.
- **Evidence (traditional):**
  - https://de.wikipedia.org/wiki/Die_Vogelhochzeit: the text dates from about 1470. Today's melody "wurde in der ersten Hälfte des 19. Jahrhunderts in Schlesien in der Gegend von Bunzlau und Haynau aufgezeichnet". The article cites Erk–Böhme, Deutscher Liederhort Bd. 1, 1893, pp. 510–517.
  - **The named print:** Hoffmann von Fallersleben and E. Richter, Schlesische Volkslieder mit Melodien, Breitkopf & Härtel, Leipzig 1842.
    - It is Nr. 43 "Vogelhochzeit" ("Es wollt' ein Vogel Hochzeit machen in dem grünen Walde. Dibiralala"), p. 72, marked "Aus der Gegend von Bunzlau und Hainau".
    - Leaf: https://archive.org/details/bub_gb_nUgJAAAAQAAJ/page/n117/
    - I opened it after logging. It is **the same tune**: 2/4 in G, an eighth B4 pickup, and the same bars 1–2 and 5–6.
    - From a rough reading, the old print varies in three places:
      - bar 3: the 2nd note is B4, where Peters has G4;
      - bar 4: "Wal-" is a dotted A4 with D5;
      - bar 7: it reads B4–C5 sixteenths, B4, A4–B4 sixteenths, A4, where Peters has B4 G4 A4 F#4.
    - So the melody's print before 1931 is secure, and the Peters "[192-?]" date no longer matters for the legal rule.
    - The `traditional` lines now cite 1842, plus de.wikipedia's Erk–Böhme 1893. The Peters line with its guessed year was removed.
- **Not to confuse:** "Rolfs Vogelhochzeit" (Rolf Zuckowski, 1977) is a different, protected work. The alias "vogelhochzeit" assumes a child means the folk song.
- **Tempo** 120 (marked "Schnell, lustig"). **Footer** "Erk Liederschatz, Peters c. 1920".

### 3. bunt-sind-schon-die-waelder: Bunt sind schon die Wälder

- **Scan:** Erk's Deutscher Liederschatz Bd. II, Nr. 18 (218) "Herbstlied", p. 19, C. F. Peters, plate 7287. The Internet Archive dates the copy 1890.
  - Page: https://archive.org/details/deutscherlieders00erkl_1/page/n26/ (PDF leaf n26).
  - The heading reads "von Salis (1782)", "Etwas lebhaft" and "J. Fr. Reichardt (1799)".
- **Read:** 6/8 in A major (3 sharps), so `bar 12` and `pickup 0`. 12 bars, verse 1.
- **Sure:** every pitch, measured. Bars 3, 5 and 11 were also checked at 200 %.
  - Slurred eighth pairs, one syllable on two notes: "schon" A4–G#4, "Stop-" C#5–B4, "Herbst" A4–C#5, "Ne-" B4–C#5, "küh-" A4–C#5 and "weht" E4–F#4.
  - Bar 5 ends on D#4 with a printed sharp.
- **Phrase ends:** bars 6 and 12 are a dotted quarter, then a quarter rest and an eighth rest (`E4:6 R:6`, `A4:6 R:6`). If score B holds the note instead, the new note-plus-rest rule makes the two equal.
- **No alias "herbstlied":** it is ambiguous, because Tieck's "Herbstlied" is in the same book.
- **Evidence:**
  - Composer: Johann Friedrich Reichardt, 25 Nov 1752 – 27 Jun 1814.
    - https://de.wikipedia.org/wiki/Johann_Friedrich_Reichardt
    - https://imslp.org/wiki/Category:Reichardt,_Johann_Friedrich (Born 1752-11-25, Died 1814-6-27)
  - Published 1799: https://de.wikipedia.org/wiki/Bunt_sind_schon_die_W%C3%A4lder ("Die Musik dazu wurde 1799 von … Reichardt komponiert"). The print's own heading also gives "(1799)".
  - Schubert's setting (D 502, 1816) is a different melody and is not used.
- **Tempo** 100. **Footer** "Erk Liederschatz II, Peters c. 1890".

### 4. hopp-hopp-hopp: Hopp, hopp, hopp, Pferdchen lauf Galopp (substitute)

- **Scan:** Unser Liederbuch: die beliebtesten Kinderlieder, Bd. 1, p. 18 "Das Steckenpferd", B. Schott's Söhne, Mainz, Copyright 1900. It is the same book as last round's Sternlein and Kinderlein.
  - Page: https://archive.org/details/unserliederbuchd01merc/page/n21/.
- **Read:** 2/4 in G major, no pickup. 12 bars, verse 1.
- **Sure:** every pitch, measured. Bars 2, 4, 10 and 12 end with a quarter rest.
- **Authorship:** the Schott page names no composer. Two independent sources name Carl Gottlieb Hering:
  - https://de.wikipedia.org/wiki/Hopp,_hopp,_hopp,_Pferdchen,_lauf_Galopp: Hering composed the melody and published it in 1809. The text is by Karl Hahn (1778–1854).
  - Erk's Deutscher Liederschatz Bd. I, Nr. 55 "Das Steckenpferd", p. 58, Peters, heading "Carl Hahn (1807)" and "Carl Gottlieb Hering (1807)": https://archive.org/details/deutscherlieders00erkl_0/page/n67/
    - I opened it after logging. Its tune is the same as Schott's, in 2/4 G major ("Munter").
    - Erk's 1807 is the composition year; de.wikipedia's 1809 is the publication year.
- **Evidence:**
  - Composer: Carl Gottlieb Hering, 25 Oct 1766 – 4 Jan 1853.
    - https://de.wikipedia.org/wiki/Carl_Gottlieb_Hering
    - https://imslp.org/wiki/Category:Hering,_Carl_Gottlieb (Born 1766-10-25, Died 1853-1-4)
  - Published 1809: the de.wikipedia song article above.
- **Not to confuse:** this is a different song from "Hoppe, hoppe Reiter". That title is not used as an alias. Neither is "das steckenpferd", the print's generic heading.
- **Tempo** 116 (marked "Keck"). **Footer** "Unser Liederbuch, Schott 1900".

### 5. winter-ade: Winter, ade (substitute)

- **Scan:** Unser Liederbuch Bd. 1, p. 47 "Winters Abschied", Schott, Mainz 1900.
  - Page: https://archive.org/details/unserliederbuchd01merc/page/n50/.
- **Read:** 3/4 in G major, no pickup. 12 bars, verse 1.
- **Sure:** every pitch.
  - The filled heads were measured.
  - The open half notes in bars 2, 4, 10 and 12 (G4, G4, D5, G4) were read at zoom against the staff lines, and so was bar 8.
  - The phrase ends are a half note plus a quarter rest (`G4:8 R:4`).
- **Evidence (traditional):**
  - https://de.wikipedia.org/wiki/Winter,_ade!_Scheiden_tut_weh: the text is by Hoffmann von Fallersleben (1835), printed with the melody in 1843. The melody is an 18th-century folk tune ("Schätzchen ade"), recorded near Würzburg by Carl Hohnbaum and first printed in Büsching's Wöchentliche Nachrichten in 1816.
  - The Schott 1900 print itself, as above.
- **No alias "winters abschied":** it is the print's generic heading, and other poems carry the same title.
- **Tempo** 92 (marked "Ruhig"). **Footer** "Unser Liederbuch, Schott 1900".

## Dropped

- **Laterne, Laterne ("Sonne, Mond und Sterne"): evidence too thin.**
  - de.wikipedia has no article on the song, only a mention in "Laternelaufen". That article's own web link says the lantern procession is no old tradition ("keine uralte Kindertradition").
  - No author or first print of the melody was found.
  - The only early hit is Böhme, Deutsches Kinderlied und Kinderspiel (Leipzig 1897), Nr. 1659 "Laternenlieder aus Berlin". It prints the words "Sonne, Mond und Sterne, ich geh' mit meiner Laterne…", which is not today's text. I did not check whether it has a melody, and it is not shown to be this tune.
  - There is no named print of the melody before 1931, so the song stays out.
- **Hoch soll er leben: evidence too thin.**
  - There is no de.wikipedia article. It is mentioned only in "Geburtstagslied" and in a Brahms anecdote (Bülow, 1892).
  - The text is attested in 19th-century newspapers.
  - None of the notated songbooks I could search have it: Erk Bd. I, II and the 1920s edition, Unser Liederbuch Bd. 1 and 2, three Kommersbuch editions (1880, 1890 and 1914), Böhme 1895, or Erk–Böhme Bd. 2 and 3.
  - With no author and no named melody print, the song stays out.
- **Why the substitutes come from outside your candidate list:**
  - None of "Grün, grün, grün", "Es tönen die Lieder", "Hoppe hoppe Reiter" or "Taler, Taler" turned up in the scanned songbooks at hand.
  - "Es tönen die Lieder" (first printed 1869, composer unknown) and "Grün, grün, grün" (current form since 1870) look legally fine, but I found no scan.
  - "Hopp, hopp, hopp" (the hobby-horse song, nearest to "Hoppe hoppe Reiter") and "Winter, ade" were in Unser Liederbuch Bd. 1 with clean evidence.
  - Neither is built in, and no name collides (the self-test checked this).

## Flagged bars (2)

1. muss-i-denn, the form: the two repeat signs, written out as A A B (see song 1).
2. die-vogelhochzeit bar 4: the fermata on D5 is ignored.

Not a bar, but also flagged: the Vogelhochzeit score A's date is only the catalogue's "[192-?]". The melody itself is safe through the 1842 print.

## Files

- Drafts: `muss-i-denn.song`, `die-vogelhochzeit.song`, `bunt-sind-schon-die-waelder.song`, `hopp-hopp-hopp.song`, `winter-ade.song`. Each now has its score-b line (see "Score B").
- Transcription log: `transcripts.md`.
- **Self-test command**, run from the repo on a scratch copy, together with the four committed songs:

  ```
  python3 -I -B scripts/make_music_data.py --library <scratchpad>/r2de/lib_selftest --out <scratchpad>/r2de/out_selftest
  ```

  It wrote 9 songs and exited 0. The only messages were the three committed `settled` notes.

## Score B (Task 9 round 2, score-B pass, 2026-11-07)

**Summary:** bunt-sind-schon-die-waelder, hopp-hopp-hopp and winter-ade **PASS**. die-vogelhochzeit and muss-i-denn are **DIFF** and need a decision from the user. No correction was made to score A, and no `settled` line was added.

### How it was done

- Every score B was copied from an existing open score, never transcribed from an image. The melody notes were copied as written. Lyrics, chords, dynamics, slurs, beams, `\tempo`, `\key`, `\time` and `\partial` were left out, and so were the other voices.
- Wikipedia wikitext came from the API (`action=parse&prop=wikitext`). Downloads went to the scratchpad (`b2de/dl`, `b2de/midi`), and only `python3 -I` was run over them.
- **Licence and origin check:**
  - The Wikipedia `<score>` blocks are CC BY-SA 4.0, like the article text. The Commons MIDI is CC0.
  - No `<score>` markup names a source edition.
  - The pages' edit comments for the score edits name no source either:
    - Vogelhochzeit: FordPrefect42, 2017-08-13, "Melodie mit score gesetzt".
    - Bunt: Michael Bednarek, 2024-11-05, "+LilyPond Melodie".
    - Hopp: FordPrefect42, 2022-08-27, "Notensatzfehler korrigiert".
    - Winter ade: Michael Bednarek, 2024-12-07, "+LilyPond Noten und Klang".
    - en "Muss i denn": Double sharp, 2019-07-28, "/* Melody */".
  - The de pages on Bunt, Hopp and Vogelhochzeit link the Liederprojekt (Carus/SWR2) under Weblinks. The Hopp and Vogelhochzeit pages also cite Weber-Kellermann, *Das Buch der Kinderlieder* (Schott 1997), for the history and under Literatur. These are citations and links, not the stated source of the score, so the scores were kept.
  - The en.wikipedia "Bunt sind schon die Wälder" score is marked "arr.: Georg L. Sothilander (2024)". It was not used: de.wikipedia's version was taken instead.
- **Scratch runs:**
  - The drafts and the four committed songs were copied to `b2de/final`. Then, from the repo root: `python3 -I -B scripts/make_music_data.py --library <scratchpad>/b2de/final --out <scratchpad>/b2de/finalout`.
  - The tool reports only the first difference in a song. So a separate diagnostic copy (`b2de/diag`, scratchpad only) changed B at the suspected places. If that copy passed, those places were shown to be the only differences.
- **Legal fields:** the tool raised no legal problem for any of the five songs. The two composers each have two URLs on different sites (de.wikipedia and IMSLP), the `published` lines are before 1931, and the `traditional` lines are dated before 1931.

### 1. muss-i-denn: DIFF (form only)

- **B:** https://en.wikipedia.org/wiki/Muss_i_denn, the `<score>` under "Melody", CC BY-SA 4.0. It is in 2/4, A major, `\partial 8`, `\language "deutsch"`, `\relative c'`. Moved −5, it matches A.
  - The de.wikipedia score is in 4/4 at doubled values and E flat major, and it splits notes for the second text line under dashed slurs. It was not used: the en score has the same values as A.
- **Copied as:** the first part is written out twice in B (verse lines 1 and 2). The second part ("Kann i glei'…") is in `\repeat volta 2 { … }` with no `\alternative`, so per the README it is stored as `\repeat unfold 2 { … }`.
- **Result:** "score B has 126 events after joining ties and folding rests, score A 86".
- **The only difference is the form.**
  - B plays A A B B. A plays A A B: the drafter played the second part once, because the stanza text is printed that way (flagged bar 1 above).
  - The diagnostic run, with B's second part played once, **passed**. Every note and length in all 86 events agrees.
  - A's own scan (n91) also shows `:||:` after bar 5 and `:||` at the very end. So the print repeats the second part too, just like both Wikipedia scores (en and de).
  - The printed text of stanzas 2 and 3 gives only one text for the second part.
- **For the user:** keep A A B (add a `settled` line), or write A as A A B B: bars 11–19 come once more, with their lead-in, so bar 19 becomes `G#4:4 R:2 E4:1 G#4:1`. The second choice makes the song PASS. I did not change A, because this is a choice of form, not a misread note.

### 2. die-vogelhochzeit: DIFF (three "Fi-di" sixteenth pairs)

- **B:** https://de.wikipedia.org/wiki/Die_Vogelhochzeit, the `<score>` under "Melodie", CC BY-SA 4.0, `\relative c''`, 2/4, G major. No transposition.
- **Result:** the first difference is at event 17. The diagnostic run, with only the three pairs below set to A's notes, passed, so these are the only differences:

  | Bar (after pickup) | Text | A (Erk/Peters, Edition Peters 395, p. 62) | B (de.wikipedia) |
  |---|---|---|---|
  | 4, last two sixteenths | "Fi-di" | D4:1 D4:1 | D5:1 B4:1 |
  | 5, last two sixteenths | "fi-di" | B4:1 B4:1 | D5:1 B4:1 |
  | 6, last two sixteenths | "fi-di" | C5:1 C5:1 | D5:1 C5:1 |

- **A re-checked against its own scan, no correction:**
  - The scan is https://archive.org/download/deutscherlieders00erkl/page/n79_w2400.jpg, cropped at x 250–1550, y 1600–1930. The crop is in the scratchpad as `b2de/scans/crop_b4_6.png`.
  - Bar 4: both sixteenths sit in the space just below the bottom staff line, so D4. They are not D5 on the fourth line, which is where the "de" eighth before them sits.
  - Bar 5: both sixteenths sit on the middle line, at the same height, so B4 B4.
  - Bar 6: the first sixteenth sits in the third space, so C5. The piano's right hand doubles the same repeated notes.
  - So A reads the print correctly. The difference is a variant between editions: the print repeats one note on "Fi-di", where Wikipedia moves down a step.
- **For the user:** keep A's version with a `settled` line, or leave the song out. NOTES.md above already says the 1842 Hoffmann/Richter print differs from Peters in other bars, and it was not used here.

### 3. bunt-sind-schon-die-waelder: PASS

- **B:** https://de.wikipedia.org/wiki/Bunt_sind_schon_die_W%C3%A4lder, the `<score>` (`sopranoVoice`), CC BY-SA 4.0, `\relative c''`, 6/8, G major. Moved +2, it matches A.
- Bars 6 and 12 are `d4. r4.` and `g4. r4.`, the same as A's `E4:6 R:6` and `A4:6 R:6`.

### 4. hopp-hopp-hopp: PASS

- **B:** https://de.wikipedia.org/wiki/Hopp,_hopp,_hopp,_Pferdchen,_lauf_Galopp, the `<score>` under "Text und Melodie", CC BY-SA 4.0, `\language "deutsch"`, `\relative g'`, 2/4, G major. No transposition.
- The de.wikibooks page "Liederbuch/ Hopp hopp hopp Pferdchen lauf Galopp" (CC BY-SA) has the same notes. It was seen, not used.

### 5. winter-ade: PASS

- **B:** https://commons.wikimedia.org/wiki/File:Winter,_ade.mid, **CC0** (`{{self|Cc-zero}}`).
  - It is by Peter Gerloff (User:Rabanus Flavus), 2022, "Melodie: volkstümlich; Satz und Tondatei: Rabanus Flavus".
  - It is in 3/4 and F major. Moved +2, it matches A.
- **Copied by script, not by ear or eye:**
  - `b2de/tools/mid2ly.py` read track 3 (the plain melody, ticks 145–289 in sixteenths). Each length is the next onset minus this one, and the last length runs to the end of bar 12.
  - Every length fell exactly on the sixteenth grid, and there are no chords on the track.
  - Track 1, bars 1–12, gives the identical 30 notes.
  - The final F4 sounds 13.3 sixteenths in the MIDI, held past the bar line. It is cut at the bar line (12).
  - B's phrase ends are dotted halves (`f'2.`), and A's `G4:8 R:4` folds to the same length.
- **Why not the de.wikipedia score:**
  - https://de.wikipedia.org/wiki/Winter,_ade!_Scheiden_tut_weh (CC BY-SA, Michael Bednarek 2024) is written in **3/8 at half values**, and the tool allows no rescaling.
  - Its pitches and rhythm match A exactly, though. A diagnostic run with its values doubled passed. So it is a second, agreeing open score.
  - Its soprano voice: `h8 h a | g4. | h8 h a | g4. | h8 c d | d c16 h c8 | a h c | c h16 a h8 | h h c | d4. | h8 h a | g4.` (`\language "deutsch"`, `\relative c''`).

### Corrections to score A

None.

### Final tool output (`b2de/final`, the five drafts and the four committed songs)

```
Nothing written. Failed:
  die-vogelhochzeit: score B differs from score A at event 17 (MIDI/sixteenths, B moved by +0), after joining ties and folding rests: A 62/1 62/1 67/2 67/2, B 74/1 71/1 67/2 67/2; fix the notes, or add a settled line saying why a person accepts the difference
  muss-i-denn: score B has 126 events after joining ties and folding rests, score A 86; fix the notes, or add a settled line saying why a person accepts the difference
Accepted difference: auld-lang-syne: score B differs from score A at event 1 (MIDI/sixteenths, B moved by +2), after joining ties and folding rests: A 62/2 67/3 67/1 67/2, B 62/4 67/6 67/2 67/4 (settled: Matthias 2026-11-07: no open score of the 2/4 version exists, so B (en.wikipedia, moved +2) is 4/4 at doubled values and also differs in bar 9 (A D5:2 B4:2 B4:2 G4:2, B halved D5:3 B4:1 B4:2 G4:2) and bar 10 (A A4:3 G4:1 A4:2 B4:2, B halved A4:3 G4:1 A4:2 E5:2); Zitro plays the old print's version (1897), and a second reading of A's scan confirmed A.)
Accepted difference: ihr-kinderlein-kommet: score B differs from score A at event 42 (MIDI/sixteenths, B moved by +2), after joining ties and folding rests: A 68/2 66/2 64/6, B 66/2 63/2 64/6 (settled: Matthias 2026-11-07: bars 15-16 ("Freude uns macht"), A (Schott 1900) has A4:4 G#4:2 F#4:2 / E4:4 R:2, B (de.wikipedia, moved +2) has A4:4 F#4:2 D#4:2 / E4:6; the two prints end differently, and it is not verified which ending is Schulz's and which is Eickhoff's; Zitro plays the old print's version, and a second reading of A's scan confirmed A.)
Accepted difference: oh-susanna: score B differs from score A at event 50 (MIDI/sixteenths, B moved by +0), after joining ties and folding rests: A 74/2 76/2 74/2 71/2, B 74/3 76/1 74/2 71/2 (settled: Matthias 2026-11-07: the Holt and Benteen 1848 editions differ in rhythm in bar 13 (A B4:2 D5:2 D5:2 E5:2, B B4:2 D5:2 D5:3 E5:1), bar 15 (A B4:2 B4:2 A4:2 A4:2, B B4:1 B4:3 A4:3 A4:1) and bar 22 (A D5:2 B4:2 G4:3 A4:1, B D5:2 B4:2 G4:2 A4:2); Zitro plays the old print's version (Holt), and a second reading of A's scan confirmed A.)
EXIT 1
```
