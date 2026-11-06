# Music band

## Requirements

- `model` value `music:<song id>` opens the music band. The id must exactly match the table in `sketch/sketch_jan12a/music_songs.h` (case aside). Anything else makes `fetch` return false, so the normal Wikipedia picture shows.
- Zitro has no recordings and can't sing. If someone asks for music outside the table, `model` stays empty and the answer must say plainly that Zitro can't play it, and name a song from the list as an example. It must never claim to play or sing. This is a prompt rule, so the wording comes from the AI and needs checking on the stick, several times per question with memory cleared (serial `forget`).
- A request for Beethoven's Ninth, or for "Beethoven's symphony" without a number, should open `ode-to-joy`. The answer must say it is the Ode to Joy melody from the Ninth, not the whole symphony.
- Measured on the stick on 2026-11-06, 12 questions 3 times each with memory cleared: no answer claimed to play something that wasn't playing. "Play Beethoven's symphony" opened the melody 2 of 3 times and "Play a Beethoven symphony" 0 of 3; the other times the answer honestly offered it. "Spiel Beethovens Sinfonie" opened it 3 of 3 times. One answer in 36 came in German to an English question. A prompt rule can't make these certain.
- Notes never come from the AI. The AI only picks an id; the prompt rule lists the ids and forbids it from supplying notes. Pitch, length, tempo and song name all come from the table.
- The table holds 26 melodies. Each is compared note by note with a published score (`tests/music/references.txt`, `tests/music/check_references.py`), and seven are also pinned by pitch and length in `tests/music/test_music.cpp`. Each song's bars must add up, with a lead-in made up by the last bar, so the loop stays in time. Every song must be public domain; see below.
- Tempo is the table's choice, not a fact from a source: scores rarely give one.
- View: a treble clef and five staff lines; real notes at their true staff positions, with stems, flags, dots, ties, sharps, flats, rests and bar lines. Notes move right to left past a fixed line 64 pixels from the left, at 10 pixels per sixteenth. Each sharp or flat is written on its note; there is no key signature.
- The note at the line lights up in citrus and sounds. Notes already played turn grey. A double bar marks where the song starts over. The band loops without end, and a song shorter than the screen fills it in both directions.
- Note names run under the notes in the device language. German: H for B natural, B for B flat, Fis, Es and so on. English: B, Bb, F#. A sixteenth has no room for its name. The sounding note's name shows large under the line.
- The bottom line shows the song name in the device language. `source` is empty: there is no outside source to name at runtime.
- Sound: `M5.Speaker.tone()` on channel 1, one note at a time, at the stored volume (`settings`, key `volume`). Tones are only started from `frame()` and each has its own length, which leaves a 12 percent gap before the next note, so nothing waits and sound can't hang when frames stop. Band and sound share one clock. A note starts on the first frame at or after its time, so up to 40 ms late.
- After a stall the clock jumps to the note it should be on and sounds only that one. Tied notes sound once for their combined length.
- `end()` stops the sound. `begin()` starts the song from its beginning, so going to the text and back starts over.
- Button B: pause, shown as two bars at the top right, and resume, which sounds the rest of the current note. Button A: new question, as everywhere. No gyro turning.
- Button C click: next volume level, round silent, low, regular, loud. Loud is the stored volume setting (`settings`, key `volume`); regular is two thirds of it and low one third. The speaker's own curve makes the steps sound further apart than that. The level's name shows for 1.5 seconds at the top right; silent stays up, because a silent band would otherwise look broken.
- Hold Button C for 1 second: back to the text. From the text, a click on Button C opens the song again from the start, at the same level.
- The level is for this song only and is never saved. A new song starts loud, and `end()` puts the speaker back to the stored volume, so speech and alarms never come out silent. An alarm during a silent song still rings at the stored volume. The user approved this change to Button C on 2026-11-06. It applies to music only: in other views a Button C click still does what it did.
- `keepAwake` is true while playing. **Addition to the brief:** after 5 minutes of playing without a Button B press it pauses by itself, and then the normal sleep timers apply. An endless melody that kept the stick awake would otherwise break the 3D spec's anti-goal of a forgotten stick draining its battery.
- A song result is not read aloud: the prompt rule sets `is_audio` false. A single click plays the music. The user approved this exception to "single click is text only".
- A double click on Button A still reads the answer aloud. `speakText` closes the view, so the first note sounds for a moment before the speech, and the song starts from the beginning after it. Fixing the moment of sound needs a change in `sketch_jan12a.ino`.
- Waking from deep sleep looks the song up again and it starts playing from the beginning, even if it was paused. Light sleep keeps the pause. Telling these apart needs a change in the shared restore code.
- Memory: notes take 1.6 KB of PSRAM, freed in `unload()`. No network and no allocation while the view is open.

## User guide

Ask for a song by name, for example "Spiel Alle meine Entchen", "Wie geht Bruder Jakob?" or "Play Silent Night". The notes move across the screen and the one on the line plays.

Zitro knows these 26 songs. It has no recordings, so for other music it tells you it can't play that and suggests a song it knows, and you still get a picture. Ask for "Ode to Joy" to hear the famous tune from Beethoven's Ninth. If you just ask for "Beethoven's symphony", Zitro may play that tune or suggest it.

German: Alle meine Entchen, Hänschen klein, Fuchs du hast die Gans gestohlen, Kommt ein Vogel geflogen, Ein Männlein steht im Walde, Alle Vögel sind schon da, Der Kuckuck und der Esel, Kuckuck Kuckuck ruft's aus dem Wald, Backe backe Kuchen, Schlaf Kindlein schlaf, Guten Abend gut' Nacht, Häschen in der Grube, Summ summ summ, O Tannenbaum, Stille Nacht, Schneeflöckchen Weißröckchen, Wer hat die schönsten Schäfchen, Hänsel und Gretel, Bruder Jakob, Zum Geburtstag viel Glück.

English and others: Twinkle Twinkle Little Star, Row Row Row Your Boat, London Bridge, Jingle Bells, Ode to Joy, Here We Go Round the Mulberry Bush.

- **Button A:** new question.
- **Button B:** pause. Press again to carry on.
- **Button C:** change the volume: silent, low, regular, loud, and round again. Each new song starts loud.
- **Hold Button C for a second:** show the text answer and stop the music. Press Button C once to hear the song from the start.

The music plays with one click on Button A, and loud means your normal volume from the settings. It keeps going round until you stop it, and pauses by itself after 5 minutes.

Jingle Bells plays the chorus only. Stille Nacht is the version sung today.

## Release note

- Ask Zitro for a song like "Alle meine Entchen": watch real notes scroll by on a never-ending band and hear each one play!

## Sources and licences

No melody below is under copyright, as far as could be confirmed from the sources named. This table comes from reading those pages, not from legal advice. Our transcriptions are written by hand and carry no text from any song. Lyrics are not stored or shown.

| Id | Melody | Why public domain |
| --- | --- | --- |
| alle-meine-entchen | German folk song, authors unknown, printed in the 19th century | Anonymous and old |
| haenschen-klein | 19th-century German song; text Franz Wiedemann (1821-1882) | Old |
| fuchs-du-hast-die-gans | Older folk song; printed 1824 with text by Ernst Anschütz (1780-1861) | Old |
| kommt-ein-vogel | Folk song, oldest known text 1824 | Old |
| ein-maennlein | Folk tune known since about 1800; text Hoffmann von Fallersleben, 1843 | Old |
| alle-voegel | Printed with these notes by 1861; text Hoffmann von Fallersleben (1798-1874), 1835 | Old |
| kuckuck-und-esel | After Carl Friedrich Zelter (1758-1832); text Hoffmann von Fallersleben, 1835 | Author long dead |
| kuckuck-ruft | Printed 1868; text Hoffmann von Fallersleben | Old |
| backe-backe-kuchen | German children's song, before 1840 | Old |
| schlaf-kindlein | Johann Friedrich Reichardt, 1781 version of a tune from 1611 | Old |
| guten-abend-gut-nacht | Johannes Brahms (1833-1897), 1868 | Author long dead |
| haeschen-in-der-grube | Folk tune; text Friedrich Fröbel, 1840 | Old |
| summ-summ-summ | Bohemian folk tune; text Hoffmann von Fallersleben | Old |
| o-tannenbaum | Folk tune, in print in 1799; text Ernst Anschütz, 1824 | Old |
| stille-nacht | Franz Xaver Gruber (1787-1863), 1818 | Author long dead |
| schneefloeckchen | Composer unknown, in songbooks since 1915 | Anonymous, published over 70 years ago |
| wer-hat-die-schoensten-schaefchen | Johann Friedrich Reichardt, 1790 | Old |
| haensel-und-gretel | Children's song, first printed 1901, no known author | Anonymous, published over 70 years ago |
| twinkle-twinkle | French "Ah! vous dirai-je, maman", published 1761 | Old |
| frere-jacques | Traditional French round | Old |
| row-your-boat | Tune first recorded with the words in 1881 | Old |
| london-bridge | Traditional English; words about 1744, today's melody recorded in the late 19th century | Old |
| jingle-bells | James Lord Pierpont (1822-1893), 1857 | Author long dead |
| ode-to-joy | Ludwig van Beethoven (1770-1827), 1824 | Author long dead |
| happy-birthday | Patty Hill (died 1946), Mildred Hill (died 1916) | EU term ended 1 January 2017; a US court ended the claim in 2015-2016 |
| mulberry-bush | English singing game, recorded in the mid-19th century | Old |

Left out on purpose:

- Wheels on the Bus (Verna Hills, 1939), If You're Happy and You Know It, Kookaburra and Itsy Bitsy Spider: could not be confirmed as public domain.
- Mary Had a Little Lamb, Old MacDonald, Three Blind Mice, Yankee Doodle, Der Mond ist aufgegangen: found no score that could be read and compared by machine, so the notes couldn't be checked this way. They can be added when one turns up.

Scores used for the check are from German and English Wikipedia and German Wikibooks, under CC BY-SA, retrieved 2026-11-06, with each page named per song in `tests/music/references.txt`. Only the notes are kept there. They are used for testing and don't ship in the firmware. A shared error in a single reference and the table would go unnoticed, and regional versions differ: these match the version in the named score.
