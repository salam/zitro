# Music band

## Requirements

- `model` value `music:<title>` opens the music band. The firmware looks the title up; the AI no longer picks an id. Design: `docs/superpowers/specs/2026-11-07-song-library-design.md`.
- **Lookup order.** `musicFindBuiltIn` (`music_songs.h`) tries the exact id, so old `music:<id>` references keep working, then the normalised title against each built-in song's names and aliases. If nothing matches, `musicFetch` (`feature_music.ino`) fetches `https://zitro.spot/data/music/index.txt` and then `<id>.txt` of the song found there. Built-in songs are found first and need no WiFi.
- **Matching** is exact on the normalised title: lower case, umlauts spelled out, accents folded, punctuation and typographic quotes dropped. There is no fuzzy matching, and a title that matches two songs matches none.
- **When no song is found** the answer is replaced by a fixed sentence from `musicMissAnswer` (`music_songs.h`), and the normal picture shows. The firmware picks the suggested song, not the AI. German: "Dieses Lied kenne ich nicht. Probier mal %s!" and, if the song list can't be reached, "Ich erreiche gerade meine Liederliste nicht. Probier mal %s!". English: "I don't know that song. Try %s!" and "I can't reach my song list right now. Try %s!". A damaged or unreadable song list counts as "can't reach". The swap in `sketch_jan12a.ino` runs whenever `model` starts with `music:` and no song is loaded, whatever the reason: a title too long for `parseModelRef` (64 bytes or more), or `create_image` or `is_wolfram` set, which skip the lookup. Without a lookup for this question the sentence is "don't know that song", never an older question's "can't reach". `memoryAdd` stores the replaced answer.
- Zitro has no recordings and can't sing. The AI sets `music:<title>` and writes only the title as its answer (`MUSIC_PROMPT_RULE`, no id list), never a promise like "Playing". If the song isn't found, the firmware replaces the answer with the fixed sentence above, so Zitro never claims to play or sing something it doesn't. The AI's wording of the title is a prompt rule and needs checking on the stick, several times per question with memory cleared (serial `forget`). Older measurements below were made under the previous rule, where the AI wrote the refusal itself.
- A request for Beethoven's Ninth, or for "Beethoven's symphony" without a number, should open `ode-to-joy` (the prompt rule gives the title "Ode an die Freude"). As for every song, the answer is only the title.
- Measured on the stick on 2026-11-06, 12 questions 3 times each with memory cleared: no answer claimed to play something that wasn't playing. "Play Beethoven's symphony" opened the melody 2 of 3 times and "Play a Beethoven symphony" 0 of 3; the other times the answer honestly offered it. "Spiel Beethovens Sinfonie" opened it 3 of 3 times. One answer in 36 came in German to an English question. A prompt rule can't make these certain.
- **Song library trials, 2026-11-07, under the current prompt rule.** Run on the stick with the firmware at this branch's state before the final fixes, device language DE, memory cleared before each ask (serial `forget`), each question asked 3 times. Every row came out the same in 3 of 3.
  - "Spiel Alle meine Entchen": `music:Alle meine Entchen`, found (built-in).
  - "Play Brother John": `music:Frere Jacques`, found.
  - "Spiel Muss i denn": the AI wrote three different title forms, all found on the server through aliases.
  - "Play Oh Susanna": `music:Oh! Susanna`, found (server).
  - "Play Beethoven's symphony": `music:Ode an die Freude`, found.
  - "Play Mozart": `music:Mozart`, unknown. "Sing mir ein Lied": `music:ein Lied`, unknown. "Play Bohemian Rhapsody": unknown. The screenshot after "Play Mozart" shows "Dieses Lied kenne ich nicht. Probier mal Alle meine Entchen!"
  - "Wer schrieb Stille Nacht?": the `model` field stayed empty.
  - The AI's answer was only the title in 27 of 27 asks, and `is_audio` was false for all 24 music asks.
  - Separate serial lookups reached all 18 server songs; 14 were checked by name, including English titles such as "Danny Boy" and "O Come, Little Children". A server lookup took about 0.75 s, a built-in 2 ms.
  - Not tested by hand yet: with WiFi off (there is no serial switch, and the offline path was seen only as a 404 before the data existed), the physical buttons, and how the new songs sound.
- Notes never come from the AI. The AI only names a title and the prompt rule forbids it from supplying notes. Pitch, length, tempo and song name all come from the built-in table or from a server song file.
- The built-in table holds 26 melodies. Each is compared note by note with a published score (`tests/music/references.txt`, `tests/music/check_references.py`), and seven are also pinned by pitch and length in `tests/music/test_music.cpp`. Each song's bars must add up, with a lead-in made up by the last bar, so the loop stays in time. Every song must be public domain; see below.
- **Server songs** live on zitro.spot (`/data/music/`), generated by `scripts/make_music_data.py` from `scripts/music_library/*.song` (format in the README there). `https://zitro.spot/data/music/SOURCES.txt` lists every server song with its sources and evidence. Eighteen are live: Auld Lang Syne; Bunt sind schon die Wälder; Camptown Races; Der Mond ist aufgegangen; Die Vogelhochzeit; Funiculì, Funiculà; Greensleeves; Hickory Dickory Dock; Hopp, hopp, hopp; Ihr Kinderlein kommet / O Come, Little Children; Londonderry Air / Danny Boy; Mary Had a Little Lamb; Muss i denn; My Bonnie; Oh! Susanna; Three Blind Mice; Weißt du, wie viel Sternlein stehen; and Winter, ade. The rules for a server song:
  - Score A comes from a pre-1931 print, or a public domain, CC0 or CC BY score. Score B is any open score and is used only to compare.
  - The two scores must agree note by note after one transposition. User decision 2026-11-07: a note followed by a rest counts as equal to a longer note of the same pitch when the next note starts at the same time.
  - Any other difference needs a `settled` line, which only the user may give. The tool finds the transposition from the whole song and lists every place where the scores differ, in its output and under the settled line in `SOURCES.txt`, so a settled line can be checked against all of them. Eight songs carry one, and each plays the version of its score A print: the Kinderlein ending, the Oh! Susanna and Camptown Races edition rhythms, Auld Lang Syne, the Vogelhochzeit "Fi-di" pairs, the Mary Had a Little Lamb and My Bonnie rhythms, and Three Blind Mice, which keeps the round's repeats.
  - Composer evidence comes from two different sites. German and English Wikipedia count as one.
  - Planned, not yet supported: the spec lets Mutopia pieces be read from one score by program. `make_music_data.py` has no such path, so today every server song needs two scores.
- Tempo is the table's choice, not a fact from a source: scores rarely give one.
- View: a treble clef and five staff lines; real notes at their true staff positions, with stems, flags, dots, ties, sharps, flats, rests and bar lines. Notes move right to left past a fixed line 64 pixels from the left, at 10 pixels per sixteenth. Each sharp or flat is written on its note; there is no key signature.
- The note at the line lights up in citrus and sounds. Notes already played turn grey. A double bar marks where the song starts over. The band loops without end, and a song shorter than the screen fills it in both directions.
- Note names run under the notes in the device language. German: H for B natural, B for B flat, Fis, Es and so on. English: B, Bb, F#. A sixteenth has no room for its name. The sounding note's name shows large under the line.
- The bottom line shows the song name in the device language. Built-in songs have an empty `source`. Server songs show theirs in the footer, for example "Oh! Susanna - Holt 1848".
- Sound: `M5.Speaker.tone()` on channel 1, one note at a time, at the stored volume (`settings`, key `volume`). Tones are only started from `frame()` and each has its own length, which leaves a 12 percent gap before the next note, so nothing waits and sound can't hang when frames stop. Band and sound share one clock. A note starts on the first frame at or after its time, so up to 40 ms late.
- After a stall the clock jumps to the note it should be on and sounds only that one. Tied notes sound once for their combined length.
- `end()` stops the sound. `begin()` starts the song from its beginning, so going to the text and back starts over.
- Button B: pause, shown as two bars at the top right, and resume, which sounds the rest of the current note. Button A: new question, as everywhere. No gyro turning.
- Button C click: next volume level, round silent, low, regular, loud. Loud is the stored volume setting (`settings`, key `volume`); regular is two thirds of it and low one third. The speaker's own curve makes the steps sound further apart than that. A tab at Button C's edge, like the ones in the print menu, shows the level: a speaker with no sound waves for silent, then one, two or three. Like every result view's tabs it shows for 3 s when the band opens and after each B or C press, then goes (changed 2026-11-07 at the user's request; it used to stay). Button B's tab shows pause, or play while paused. Silent starts no tones at all, because the buzzer clicks when a tone starts, even at volume 0. The tab sits at the top left, or at the bottom right when the stick is held the other way round; while it shows it covers the end of a long song name.
- Button B double click: coloured note heads on or off, saved in `settings`, key `musicColors` (default off), so it carries over to the next song and survives sleep. The colours are the Boomwhackers rainbow exactly as Soundslice names it: C red, D orange, E yellow, F green, G teal, A indigo, B violet (see Sources). There is no international standard; other schemes, such as Figurenotes, differ. Indigo and violet are drawn brighter than their names suggest so they read on black. A sharp or flat takes its written letter's colour, since the head sits on that letter's place. Only the head is coloured: stems, flags, dots, ties, sharps, flats, rests and names stay as before. Played heads keep their colour at half brightness. Citrus is too close to the yellow E, so the sounding note gets a citrus ring around its head instead. Half and whole notes stay hollow.
- The double click is M5Unified's: it is decided once Button B has been still for its hold time of 1 second, so the colours switch about a second after the second click. Each of the two clicks still pauses or resumes, so the song stops for a moment and then carries on as before. It follows that pausing and resuming within a second also switches the colours, and the change is saved. The new `EduFeature::buttonBDouble` is nullptr in every other view, where a double click does nothing extra.
- Hold Button C for 1 second: back to the text. From the text, a click on Button C opens the song again from the start, at the same level.
- The level is for this song only and is never saved. A new song starts loud, and `end()` puts the speaker back to the stored volume, so speech and alarms never come out silent. An alarm during a silent song still rings at the stored volume. The user approved this change to Button C on 2026-11-06. It applies to music only: in other views a Button C click still does what it did.
- `keepAwake` is true while playing. **Addition to the brief:** after 5 minutes of playing without a Button B press it pauses by itself, and then the normal sleep timers apply. An endless melody that kept the stick awake would otherwise break the 3D spec's anti-goal of a forgotten stick draining its battery.
- A song result is not read aloud: the prompt rule sets `is_audio` false. A single click plays the music. The user approved this exception to "single click is text only".
- A double click on Button A still reads the answer aloud. `speakText` closes the view, so the first note sounds for a moment before the speech, and the song starts from the beginning after it. Fixing the moment of sound needs a change in `sketch_jan12a.ino`.
- Waking from deep sleep looks the song up again and it starts playing from the beginning, even if it was paused. Light sleep keeps the pause. Telling these apart needs a change in the shared restore code.
- Memory: `MUSIC_MAX_NOTES` is 1000 notes, up to 8 KB of PSRAM (the old built-in table needed 1.6 KB), freed in `unload()`. The network is used only while looking a song up, never while the view is open. Built-in songs work without WiFi.

## User guide

Ask for a song by name, for example "Spiel Alle meine Entchen", "Wie geht Bruder Jakob?" or "Play Silent Night". The notes move across the screen and the one on the line plays.

Zitro knows 26 songs by heart, listed below, and works without WiFi for those. With WiFi it finds 18 more on its own server, such as Oh! Susanna and Der Mond ist aufgegangen; `https://zitro.spot/data/music/SOURCES.txt` says where each of those comes from. It has no recordings. If it doesn't know a song it says "I don't know that song. Try ...!" and suggests one, and if it can't reach its song list it says so, and you still get a picture. Ask for "Ode to Joy" to hear the famous tune from Beethoven's Ninth. If you just ask for "Beethoven's symphony", Zitro may play that tune, or say it doesn't know it and suggest another song.

German: Alle meine Entchen, Hänschen klein, Fuchs du hast die Gans gestohlen, Kommt ein Vogel geflogen, Ein Männlein steht im Walde, Alle Vögel sind schon da, Der Kuckuck und der Esel, Kuckuck Kuckuck ruft's aus dem Wald, Backe backe Kuchen, Schlaf Kindlein schlaf, Guten Abend gut' Nacht, Häschen in der Grube, Summ summ summ, O Tannenbaum, Stille Nacht, Schneeflöckchen Weißröckchen, Wer hat die schönsten Schäfchen, Hänsel und Gretel, Bruder Jakob, Zum Geburtstag viel Glück.

English and others: Twinkle Twinkle Little Star, Row Row Row Your Boat, London Bridge, Jingle Bells, Ode to Joy, Here We Go Round the Mulberry Bush.

- **Button A:** new question.
- **Button B:** pause. Press again to carry on.
- **Double-click Button B:** colourful note heads on or off. Every note gets its own colour, as on Boomwhackers: C red, D orange, E yellow, F green, G teal, A indigo, B violet (H in German). A sharp or flat keeps the colour of its letter, so F♯ is green. The colours switch about a second after your double click, and the song only stops for a moment. Zitro remembers your choice for the next song. Pausing and carrying on within a second counts as a double click too, so if the colours appear by surprise, double-click again.
- **Button C:** change the volume: silent, low, regular, loud, and round again. The speaker symbol at the button shows the level for a moment: no sound waves means silent. Each new song starts loud.
- **Hold Button C for a second:** show the text answer and stop the music. Press Button C once to hear the song from the start.

The music plays with one click on Button A, and loud means your normal volume from the settings. It keeps going round until you stop it, and pauses by itself after 5 minutes.

Jingle Bells plays the chorus only. Stille Nacht is the version sung today.

## Release note

- Ask Zitro for a song like "Alle meine Entchen": watch real notes scroll by on a never-ending band and hear each one play!
- More songs from zitro.spot, and an honest "I don't know that song. Try ...!" when Zitro has no match. See `RELEASE_NOTES.md`.
- **Notes in colour**: double-click Button B while a song plays and every note head lights up in its own rainbow colour, C red to B violet, just like Boomwhackers.

## Sources and licences

On 2026-11-07 every melody below was checked against the legal rule of the song library (rules 1 to 3 and the evidence rule in `docs/superpowers/specs/2026-11-07-song-library-design.md`). A song passes if everyone who wrote the melody died before 1926, or if the melody is traditional and was printed before 1931. Two independent sources must agree on the author and their dates. German and English Wikipedia together count as one source. Lyrics don't count, because Zitro never stores or shows them. This is a careful reading of the sources named, not legal advice. Our transcriptions are typed in by hand and carry no text from any song.

Numbers point to the list under the table. "Scan checked" means the page of the old print was looked at and its tune matches ours, allowing for key and for verse repeats.

| Id | Melody: author and dates, or traditional with an early print | Sources | Verdict |
| --- | --- | --- | --- |
| alle-meine-entchen | Traditional; the tune is in dance manuscripts from about 1750. Printed with notes in Böhme, *Deutsches Kinderlied und Kinderspiel*, 1897, p. 502, no. 254 (scan checked) | 8, 2; print 1 | passes |
| haenschen-klein | Traditional, the hunting song "Fahret hin"; tune printed 1807 in Büsching and von der Hagen, *Sammlung deutscher Volkslieder*, Melodienheft, p. 11 | 9, 10; print 11 | passes |
| fuchs-du-hast-die-gans | Traditional, "Wer die Gans gestohlen hat"; printed 1824 in Anschütz, *Musikalisches Schulgesangbuch* 1, p. 38 (in numbers), and with notes in Böhme 1897, p. 577 (scan checked) | 12, 2; print 13, 1 | passes |
| kommt-ein-vogel | Traditional Austrian; Wenzel Müller (died 1835) is named as its alleged composer. The old prints seen carry an older form that differs from ours in "auf mein Fuß": Erk and Böhme, *Deutscher Liederhort* 2, Leipzig 1893, p. 790, no. 1051 (scan checked), and *Allgemeines Deutsches Kommersbuch*, Lahr 1914, p. 524, no. 572 (scan checked). No print of our form was found | 14, 2, 4; print 63, 64 | **unclear** |
| ein-maennlein | Traditional, known since about 1800; printed as "Volksweise" in *Deutsche Weisen*, Stuttgart [1900], no. 261 | 15, 5; print 5 | passes |
| alle-voegel | Traditional, after the late-18th-century tune "Nun so reis ich fort"; printed with this text in Hamburg in 1844, and as "Volksmelodie" in Hoffmann von Fallersleben, *Vierzig Kinderlieder*, Leipzig 1847, no. 33 | 16, 2, 17; print 4 | passes |
| kuckuck-und-esel | Carl Friedrich Zelter (1758-1832), adapted from his 1810 setting of "Es ist ein Schuss gefallen" | 18, 19, 2, 4 | passes |
| kuckuck-ruft | Traditional Austrian, from "Stieglitz, Stieglitz, 's Zeiserl is krank" (1817); printed as "Volksmelodie" in Hoffmann von Fallersleben and Richter, *Fünfzig Kinderlieder*, Leipzig 1843, no. 2 | 20, 2; print 4, 21 | passes |
| backe-backe-kuchen | Traditional, from Saxony and Thuringia, before 1840; printed with notes in Böhme 1897, p. 46, no. 174 (scan checked) | 22, 2; print 1 | passes |
| schlaf-kindlein | Johann Friedrich Reichardt (1752-1814), 1781, after a folk tune; first in Reichardt, *Lieder für Kinder*, Hamburg 1781; also in Böhme 1897, p. 1, no. 1 (scan checked) | 23, 24, 1, 4 | passes |
| guten-abend-gut-nacht | Johannes Brahms (1833-1897), op. 49 no. 4, Simrock, Berlin 1868 | 25, 26, 27 | passes |
| haeschen-in-der-grube | Traditional, "Wer die Gans gestohlen hat", per Wikipedia and Böhme 1897, p. 577, no. 409 (scan checked); Holzapfel and the Liederprojekt name Karl Enslin (1819-1875) | 28, 1, 2, 29 | passes |
| summ-summ-summ | Traditional, from Bohemia; printed in Hoffmann von Fallersleben and Richter, *Fünfzig Kinderlieder*, Leipzig 1843, p. 49 | 30, 31; print 21 | passes |
| o-tannenbaum | Traditional, a tune from about 1700 sung to student songs; printed in Kretzschmer and Zuccalmaglio, 1840, no. 263, and as "Lauriger Horatius" in Fink, *Musikalischer Hausschatz der Deutschen*, Leipzig 1843, p. 289 | 32, 3; print 33 | passes |
| stille-nacht | Franz Xaver Gruber (1787-1863), 1818; first printed by Friese, Dresden 1833; the form sung today settled in the 1840s | 34, 35, 36 | passes |
| schneefloeckchen | Composer unknown. The sources disagree on when today's tune appeared: 1915, 1927 or about 1945 | 37, 3, 38 | **unclear** |
| wer-hat-die-schoensten-schaefchen | Johann Friedrich Reichardt (1752-1814), written in 1790 for another song | 39, 40, 3 | passes |
| haensel-und-gretel | Author unknown; first printed 1901 by Paul Hoffmann, Halle, who may have written it | 41, 42, 2 | **unclear** |
| twinkle-twinkle | Traditional French, "Ah! vous dirai-je, maman", first published 1761; the theme of Mozart's variations K. 265, published 1785 | 43, 44, 45 | passes |
| frere-jacques | Traditional French, in a manuscript from about 1780 and first printed 1811 (*La Clé du Caveau*, Paris); some credit Jean-Philippe Rameau (died 1764). Printed as "French Air" in *The Golden Book of Favorite Songs*, Chicago 1923, p. 121 (scan checked) | 46, 6 | passes |
| row-your-boat | Eliphalet Oram Lyte (1842-1913), credited in *Franklin Square Song Collection*, Harper, New York 1881, p. 69 (scan checked) | 47, 48, 49; print 50 | passes |
| london-bridge | Traditional; today's tune first printed 1879 in A. H. Rosewig, *Illustrated National Songs and Games*, Philadelphia (not seen). Printed note for note as ours in Bancroft, *Games for the Playground, Home, School and Gymnasium*, Macmillan, New York, 1920 (copyright 1909), p. 278 (scan checked). Gomme 1894, p. 333 and Hofer 1914, p. 13 print it with a different last bar | 51, 7; print 65, 66 | passes |
| jingle-bells | James Lord Pierpont (1822-1893), Oliver Ditson, Boston 1857. We play the later chorus, whose origin is unknown; it was recorded by 1898 and is printed, credited to Pierpont, in *The Golden Book of Favorite Songs*, Chicago 1923, p. 120 (scan checked) | 52, 53, 54; print 6 | passes |
| ode-to-joy | Ludwig van Beethoven (1770-1827), Ninth Symphony, finished 1824, printed by Schott 1826 | 55, 56, 57 | passes |
| happy-birthday | Mildred J. Hill (1859-1916), the tune of "Good Morning to All" in *Song Stories for the Kindergarten*, Chicago 1893. Her sister Patty Hill (died 1946) wrote the words, which Zitro doesn't store | 58, 59, 60, 61 | passes |
| mulberry-bush | Traditional, the 18th-century tune "Nancy Dawson"; printed in Gomme, *Traditional Games* 1, London 1894, p. 404 (scan checked) | 62, 7 | passes |

Fails or unclear (kept for now; nothing is removed without asking):

- **schneefloeckchen:** The composer is unknown, so only an early print can show the tune is old. German Wikipedia finds today's tune in songbooks from 1915. Holzapfel finds it in songbooks only from 1927, with one record from 1917. The Liederprojekt says today's version has been sung since about 1945. If the 1945 date is right, the tune may be under copyright.
- **haensel-und-gretel:** Nobody is known to have written the tune. It was first printed in 1901 in Paul Hoffmann's school songbook, and the Liederprojekt says Hoffmann may have written it. His dates are not known, so we can't tell whether he died before 1926.
- **kommt-ein-vogel:** The song is old and traditional, but the two prints from before 1931 that were checked go stepwise up to the third on "auf mein Fuß", where we leap up to the fifth and come down. Two notes differ. No print of the form we play from before 1931 was found, and German Wikipedia names none. The composer path doesn't settle it either: Holzapfel calls Wenzel Müller only the alleged composer, and Müller's credit doesn't show which form he wrote.

Where the sources differ but the verdict holds:

- **haeschen-in-der-grube:** a folk tune, or Karl Enslin (1819-1875); either way it passes.
- **o-tannenbaum:** a folk tune; Holzapfel names Ernst Anschütz (1780-1861) as "Komp."; either way it passes.
- **alle-voegel:** a folk tune; a 1919 songbook credits Mozart, which Holzapfel calls wrong; Mozart died in 1791, so it passes either way.
- **wer-hat-die-schoensten-schaefchen:** today's tune is Reichardt's; Karl von Winterfeld (1831) and Carl Reinecke (1824-1910) wrote other tunes to the same words.
- **frere-jacques:** a folk tune, or Jean-Philippe Rameau (1683-1764); either way it passes.
- **row-your-boat:** the 1881 print credits Lyte, but it is unclear whether he wrote the tune or arranged it. If he wrote it, he died in 1913. If he arranged an older tune, that tune has no known author and the 1881 print is from before 1931, so it passes as traditional.
- **jingle-bells:** Pierpont's 1857 chorus differs from the one sung today. Pierpont died in 1893, so his verse passes. The later chorus we play has no known author, was recorded by 1898 and is printed in a named book from 1923, so it passes as traditional. This is the closest pass in the table.
- **happy-birthday:** one writer argues that the Hill sisters adapted older songs from 1858 to 1885. Another says those songs had different tunes. Every candidate author died before 1926.

Sources, retrieved 2026-11-07:

1. Franz Magnus Böhme, *Deutsches Kinderlied und Kinderspiel*, Leipzig 1897: https://archive.org/details/DeutschesKinderliedUndKinderspiel (p. 1, 46, 502 and 577 are scan pages n77, n122, n578 and n653)
2. Otto Holzapfel, *Liedverzeichnis*, Lieder A–K, update March 2024, from the catalogue of the Deutsches Volksliedarchiv: https://volksmusik-forschung.de/assets/files/Downloads/Holzapfel_Lieder_A_K.pdf
3. Otto Holzapfel, *Liedverzeichnis*, Lieder L–Z: https://volksmusik-forschung.de/assets/files/Downloads/Holzapfel_Lieder_L_Z.pdf
4. Hoffmann von Fallersleben and Prahl, *Unsere volkstümlichen Lieder*, 4th edition, Leipzig 1900, nos. 42, 203, 775, 790 and 994: https://archive.org/details/unserevolkstmli00unkngoog
5. IMSLP, *Deutsche Weisen*, edited by August Linder, Stuttgart [1900]: https://imslp.org/wiki/Deutsche_Weisen_(Various)
6. *The Golden Book of Favorite Songs*, Hall & McCreary, Chicago 1923: https://archive.org/details/goldenbookoffavo00beat_0
7. Alice B. Gomme, *The Traditional Games of England, Scotland, and Ireland*, vol. 1, London 1894: https://archive.org/details/traditionalgames01gommuoft
8. https://de.wikipedia.org/wiki/Alle_meine_Entchen
9. https://de.wikipedia.org/wiki/H%C3%A4nschen_klein
10. https://liederprojekt.org/lied30300-Haenschen_klein_ging_allein.html
11. https://books.google.de/books?id=PuFNAAAAcAAJ&pg=PA11
12. https://de.wikipedia.org/wiki/Fuchs,_du_hast_die_Gans_gestohlen
13. http://diglib.hab.de/show_image.php?dir=drucke/un-17-1b&pointer=47
14. https://de.wikipedia.org/wiki/Kommt_ein_Vogel_geflogen
15. https://de.wikipedia.org/wiki/Ein_M%C3%A4nnlein_steht_im_Walde
16. https://de.wikipedia.org/wiki/Alle_V%C3%B6gel_sind_schon_da
17. https://www.liederprojekt.org/lied30269-Alle-Voegel-sind-schon-da.html
18. https://de.wikipedia.org/wiki/Der_Kuckuck_und_der_Esel
19. https://de.wikipedia.org/wiki/Carl_Friedrich_Zelter
20. https://de.wikipedia.org/wiki/Kuckuck,_Kuckuck,_ruft%E2%80%99s_aus_dem_Wald
21. https://gdz.sub.uni-goettingen.de/dms/load/img/?PPN=PPN362458375
22. https://de.wikipedia.org/wiki/Backe,_backe_Kuchen
23. https://de.wikipedia.org/wiki/Schlaf,_Kindlein,_schlaf
24. https://de.wikipedia.org/wiki/Johann_Friedrich_Reichardt
25. https://de.wikipedia.org/wiki/Guten_Abend,_gut%E2%80%99_Nacht
26. https://imslp.org/wiki/5_Lieder,_Op.49_(Brahms,_Johannes)
27. https://imslp.org/wiki/Category:Brahms,_Johannes
28. https://de.wikipedia.org/wiki/H%C3%A4schen_in_der_Grube
29. https://www.liederprojekt.org/lied30247-Haeschen-in-der-Grube.html
30. https://de.wikipedia.org/wiki/Summ,_summ,_summ
31. https://www.liederprojekt.org/lied29905-Summ-summ-summ.html
32. https://de.wikipedia.org/wiki/O_Tannenbaum
33. https://books.google.de/books?id=571fAAAAcAAJ&pg=PA289
34. https://en.wikipedia.org/wiki/Silent_Night
35. https://imslp.org/wiki/Category:Gruber,_Franz_Xaver
36. https://hymnary.org/tune/stille_nacht_gruber
37. https://de.wikipedia.org/wiki/Schneefl%C3%B6ckchen,_Wei%C3%9Fr%C3%B6ckchen
38. https://www.liederprojekt.org/lied30282.html
39. https://de.wikipedia.org/wiki/Wer_hat_die_sch%C3%B6nsten_Sch%C3%A4fchen
40. https://www.liederprojekt.org/lied27788.html
41. https://de.wikipedia.org/wiki/H%C3%A4nsel_und_Gretel_verliefen_sich_im_Wald
42. https://liederprojekt.org/lied30306-Haensel-und-Gretel.html
43. https://en.wikipedia.org/wiki/Twinkle,_Twinkle,_Little_Star
44. https://en.wikipedia.org/wiki/Ah!_vous_dirai-je,_maman
45. https://imslp.org/wiki/12_Variations_on_'Ah,_vous_dirai-je_maman',_K.265/300e_(Mozart,_Wolfgang_Amadeus)
46. https://en.wikipedia.org/wiki/Fr%C3%A8re_Jacques
47. https://en.wikipedia.org/wiki/Row,_Row,_Row_Your_Boat
48. https://en.wikipedia.org/wiki/Eliphalet_Oram_Lyte
49. Library of Congress name authority "Lyte, Eliphalet Oram, 1842-1913": https://id.loc.gov/authorities/names/no2018014532
50. *Franklin Square Song Collection*, selected by J. P. McCaskey, Harper, New York 1881: https://archive.org/details/franklinsquares04mccagoog
51. https://en.wikipedia.org/wiki/London_Bridge_Is_Falling_Down
52. https://en.wikipedia.org/wiki/Jingle_Bells
53. https://en.wikipedia.org/wiki/James_Lord_Pierpont
54. https://imslp.org/wiki/The_One_Horse_Open_Sleigh_(Pierpont,_James)
55. https://en.wikipedia.org/wiki/Ode_to_Joy
56. https://imslp.org/wiki/Symphony_No.9,_Op.125_(Beethoven,_Ludwig_van)
57. https://imslp.org/wiki/Category:Beethoven,_Ludwig_van
58. https://en.wikipedia.org/wiki/Happy_Birthday_to_You
59. https://en.wikipedia.org/wiki/Mildred_J._Hill
60. https://imslp.org/wiki/Song_Stories_for_the_Kindergarten_(Hill,_Mildred)
61. https://imslp.org/wiki/Category:Hill,_Mildred
62. https://en.wikipedia.org/wiki/Here_We_Go_Round_the_Mulberry_Bush
63. Erk and Böhme, *Deutscher Liederhort*, vol. 2, Breitkopf & Härtel, Leipzig 1893, p. 790, no. 1051: https://archive.org/details/bub_gb_dePoAAAAIAAJ/page/n799
64. *Allgemeines Deutsches Kommersbuch*, Schauenburg, Lahr 1914, p. 524, no. 572: https://archive.org/details/allgemeinesdeuts00silc/page/n541
65. Jessie H. Bancroft, *Games for the Playground, Home, School and Gymnasium*, Macmillan, New York, 1920 (copyright 1909), p. 278: https://archive.org/details/GamesForThePlayground/page/n319
66. Mari Ruef Hofer, *Children's Singing Games, Old and New*, Chicago 1914, p. 13: https://archive.org/details/ChildrensSingingGames/page/n15

Left out on purpose:

- Wheels on the Bus (Verna Hills, 1939), If You're Happy and You Know It, Kookaburra and Itsy Bitsy Spider: could not be confirmed as public domain.
- Old MacDonald, Yankee Doodle: found no score that could be read and compared by machine, so the notes couldn't be checked this way. They can be added when one turns up.

Note head colours: Soundslice, "Using Boomwhackers colors", https://www.soundslice.com/help/en/player/advanced/336/boomwhackers-colors/, retrieved 2026-11-07. It lists C Red, D Orange, E Yellow, F Green, G Teal, A Indigo, B Violet, with no hex values and nothing on sharps or flats. Other sellers and teachers name G, A and B differently (dark green, purple, magenta); this table follows Soundslice alone rather than mixing them. The colour values are our own.

Scores used for the check are from German and English Wikipedia and German Wikibooks, under CC BY-SA, retrieved 2026-11-06, with each page named per song in `tests/music/references.txt`. Only the notes are kept there. They are used for testing and don't ship in the firmware. A shared error in a single reference and the table would go unnoticed, and regional versions differ: these match the version in the named score.
