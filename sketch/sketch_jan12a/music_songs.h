// Melodies Zitro can play: music:<id>. Written by hand, compared note by note against published scores
// by tests/music/run.sh, and all in the public domain: see docs/features/music.md.
// The AI only picks an id. Notes never come from the AI.
#pragma once

#include <stdint.h>

struct MusicSong {
  const char* id;
  const char* nameDe;
  const char* nameEn;
  uint8_t beatsPerMinute;  // Quarter notes
  uint8_t barTicks;        // Length of a bar in sixteenths
  uint8_t pickupTicks;     // Lead-in before the first bar line; the last bar makes up for it
  const char* notes;       // See music_core.h
};

static const MusicSong MUSIC_SONGS[] = {
    {"alle-meine-entchen", "Alle meine Entchen", "Alle meine Entchen", 104, 16, 0,
     "C4:2 D4:2 E4:2 F4:2 G4:4 G4:4 | A4:2 A4:2 A4:2 A4:2 G4:8 | A4:2 A4:2 A4:2 A4:2 G4:8 | "
     "F4:2 F4:2 F4:2 F4:2 E4:4 E4:4 | D4:2 D4:2 D4:2 D4:2 C4:8"},
    {"haenschen-klein", "H\xC3\xA4nschen klein", "H\xC3\xA4nschen klein", 100, 8, 0,
     "C5:2 A4:2 A4:4 | Bb4:2 G4:2 G4:4 | F4:2 G4:2 A4:2 Bb4:2 | C5:2 C5:2 C5:4 | "
     "C5:2 A4:2 A4:4 | Bb4:2 G4:2 G4:4 | F4:2 A4:2 C5:2 C5:2 | F4:4 R:4 | "
     "G4:2 G4:2 G4:2 G4:2 | G4:2 A4:2 Bb4:4 | A4:2 A4:2 A4:2 A4:2 | A4:2 Bb4:2 C5:4 | "
     "C5:2 A4:2 A4:4 | Bb4:2 G4:2 G4:4 | F4:2 A4:2 C5:2 C5:2 | F4:8"},
    {"fuchs-du-hast-die-gans", "Fuchs, du hast die Gans gestohlen", "Fuchs, du hast die Gans gestohlen", 120, 16, 0,
     "D4:4 E4:4 F#4:4 G4:4 | A4:4 A4:4 A4:4 A4:4 | B4:4 G4:4 D5:4 B4:4 | A4:16 | B4:4 G4:4 D5:4 B4:4 | A4:16 | "
     "A4:4 G4:4 G4:4 G4:4 | G4:4 F#4:4 F#4:4 F#4:4 | F#4:4 E4:4 F#4:4 E4:4 | D4:4 F#4:4 A4:8 | "
     "A4:4 G4:4 G4:4 G4:4 | G4:4 F#4:4 F#4:4 F#4:4 | F#4:4 E4:4 F#4:4 E4:4 | D4:16"},
    {"kommt-ein-vogel", "Kommt ein Vogel geflogen", "Kommt ein Vogel geflogen", 120, 12, 4,
     "A4:2 Bb4:2 | C5:4 A4:4 A4:4 | A4:4 G4:4 G4:2 A4:2 | Bb4:4 G4:4 G4:2 D5:2 | C5:4 R:4 A4:2 Bb4:2 | "
     "C5:4 A4:4 A4:4 | A4:4 G4:4 G4:2 A4:2 | Bb4:4 E4:4 E4:4 | F4:4 R:4"},
    {"ein-maennlein", "Ein M\xC3\xA4nnlein steht im Walde", "Ein M\xC3\xA4nnlein steht im Walde", 132, 16, 4,
     "A3:4 | D4:4 E4:4 F#4:4 G4:4 | A4:8 B4:4 G4:4 | F#4:8 E4:8 | D4:8 R:4 A3:4 | "
     "D4:4 E4:4 F#4:4 G4:4 | A4:8 B4:4 G4:4 | F#4:8 E4:8 | D4:8 R:8 | "
     "A4:6 G4:2 F#4:4 A4:4 | G4:4 F#4:4 E4:8 | A4:6 G4:2 F#4:4 A4:4 | G4:4 F#4:4 E4:8 | "
     "D4:4 E4:4 F#4:4 G4:4 | A4:8 B4:4 G4:4 | F#4:8 E4:8 | D4:8 R:4"},
    {"alle-voegel", "Alle V\xC3\xB6gel sind schon da", "Alle V\xC3\xB6gel sind schon da", 120, 16, 0,
     "D4:6 F#4:2 A4:4 D5:4 | B4:4 D5:2 B4:2 A4:8 | G4:6 A4:2 F#4:4 D4:4 | E4:8 D4:4 R:4 | "
     "A4:4 A4:4 G4:4 G4:4 | F#4:4 A4:2 F#4:2 E4:8 | A4:4 A4:4 G4:4 G4:4 | F#4:4 A4:2 F#4:2 E4:8 | "
     "D4:6 F#4:2 A4:4 D5:4 | B4:4 D5:2 B4:2 A4:8 | G4:6 A4:2 F#4:4 D4:4 | E4:8 D4:4 R:4"},
    {"kuckuck-und-esel", "Der Kuckuck und der Esel", "Der Kuckuck und der Esel", 132, 16, 4,
     "C5:4 | A4:4 C5:4 A4:4 C5:4 | Bb4:4 Bb4:4 R:4 Bb4:4 | G4:4 Bb4:4 G4:4 Bb4:4 | A4:8 R:4 C5:2 Bb4:2 | "
     "A4:4 A4:4 A4:4 A4:4 | Bb4:4 Bb4:4 R:4 Bb4:2 A4:2 | G4:4 G4:4 G4:4 G4:4 | A4:4 A4:4 R:4 F4:4 | "
     "F4:4 G4:4 A4:4 Bb4:4 | C5:6 D5:2 C5:4 Bb4:4 | A4:4 A4:4 G4:4 G4:4 | F4:8 R:4"},
    {"kuckuck-ruft", "Kuckuck, Kuckuck, ruft's aus dem Wald", "Kuckuck, Kuckuck, ruft's aus dem Wald", 132, 12, 0,
     "D5:4 B4:4 R:4 | D5:4 B4:4 R:4 | A4:4 G4:4 A4:4 | G4:12 | A4:4 A4:4 B4:4 | C5:8 A4:4 | "
     "B4:4 B4:4 C5:4 | D5:8 B4:4 | D5:8 B4:4 | D5:8 B4:4 | C5:4 B4:4 A4:4 | G4:12"},
    {"backe-backe-kuchen", "Backe, backe Kuchen", "Backe, backe Kuchen", 132, 16, 0,
     "A4:4 A4:4 B4:4 B4:4 | A4:8 F#4:4 D4:4 | A4:4 A4:4 B4:4 B4:4 | A4:8 F#4:8 | "
     "A4:4 A4:4 B4:4 B4:4 | A4:4 A4:4 F#4:4 D4:4 | A4:4 A4:4 B4:4 B4:4 | A4:4 A4:4 F#4:4 F#4:4 | "
     "A4:2 A4:2 A4:4 F#4:8 | A4:2 A4:2 A4:4 F#4:8 | A4:4 A4:4 F#4:8 | "
     "A4:4 A4:4 B4:4 B4:4 | A4:4 A4:4 F#4:8 | D5:8 A4:4 G4:4 | F#4:4 E4:4 D4:8"},
    {"schlaf-kindlein", "Schlaf, Kindlein, schlaf", "Schlaf, Kindlein, schlaf", 72, 8, 0,
     "A4:4 G4:2 G4:2 | F4:4 R:2 F4:1 A4:1 | C5:2 C5:2 Bb4:2 Bb4:2 | A4:4 R:2 A4:2 | Bb4:2 Bb4:2 G4:2 G4:2 | "
     "C5:2 C5:2 A4:2 A4:2 | Bb4:2 Bb4:2 G4:2 G4:2 | C5:2 C5:2 A4:4 | Bb4:4 G4:2 G4:2 | F4:4 R:4"},
    {"guten-abend-gut-nacht", "Guten Abend, gut' Nacht", "Brahms' Lullaby", 76, 12, 4,
     "G4:2 G4:2 | Bb4:6 G4:2 G4:4 | Bb4:4 R:4 G4:2 Bb4:2 | Eb5:4 D5:6 C5:2 | C5:4 Bb4:4 F4:2 G4:2 | "
     "Ab4:4 F4:4 F4:2 G4:2 | Ab4:4 R:4 F4:2 Ab4:2 | D5:2 C5:2 Bb4:4 D5:4 | Eb5:4 R:4 Eb4:2 Eb4:2 | "
     "Eb5:8 C5:2 Ab4:2 | Bb4:8 G4:2 Eb4:2 | Ab4:4 Bb4:4 C5:4 | Bb4:8 Eb4:2 Eb4:2 | "
     "Eb5:8 C5:2 Ab4:2 | Bb4:8 G4:2 Eb4:2 | Ab4:4 G4:4 F4:4 | Eb4:8"},
    {"haeschen-in-der-grube", "H\xC3\xA4schen in der Grube", "H\xC3\xA4schen in der Grube", 100, 8, 0,
     "D4:2 E4:2 F#4:2 G4:2 | A4:4 A4:4 | B4:2 G4:2 D5:2 B4:2 | A4:4 R:4 | B4:2 G4:2 D5:2 B4:2 | A4:4 R:4 | "
     "A4:2 G4:2 G4:2 G4:2 | G4:2 F#4:2 F#4:4 | F#4:2 E4:2 F#4:2 E4:2 | D4:2 F#4:2 A4:4 | "
     "A4:2 A4:2 D5:4 | A4:2 A4:2 D5:4 | A4:4 A4:4 | D4:4 R:4"},
    {"summ-summ-summ", "Summ, summ, summ", "Summ, summ, summ", 104, 8, 0,
     "D5:4 C5:4 | B4:4 R:4 | A4:2 B4:2 C5:2 A4:2 | G4:4 R:4 | B4:2 C5:2 D5:2 B4:2 | A4:2 B4:2 C5:2 A4:2 | "
     "B4:2 C5:2 D5:2 B4:2 | A4:2 B4:2 C5:2 A4:2 | D5:4 C5:4 | B4:4 R:4 | A4:2 B4:2 C5:2 A4:2 | G4:4 R:4"},
    {"o-tannenbaum", "O Tannenbaum", "O Christmas Tree", 88, 12, 2,
     "D4:2 | G4:3 G4:1 G4:4 A4:4 | B4:3 B4:1 B4:6 B4:2 | A4:2 B4:2 C5:4 F#4:4 | A4:4 G4:4 R:2 "
     "D5:2 | D5:2 B4:2 E5:6 D5:2 | D5:2 C5:2 C5:6 C5:2 | C5:2 A4:2 D5:6 C5:2 | C5:2 B4:2 B4:4 R:2 "
     "D4:2 | G4:3 G4:1 G4:4 A4:4 | B4:3 B4:1 B4:6 B4:2 | A4:2 B4:2 C5:4 F#4:4 | A4:4 G4:4 R:2"},
    {"stille-nacht", "Stille Nacht", "Silent Night", 66, 12, 0,
     "G4:3 A4:1 G4:2 E4:6 | G4:3 A4:1 G4:2 E4:6 | D5:4 D5:2 B4:6 | C5:4 C5:2 G4:6 | "
     "A4:4 A4:2 C5:3 B4:1 A4:2 | G4:3 A4:1 G4:2 E4:6 | A4:4 A4:2 C5:3 B4:1 A4:2 | G4:3 A4:1 G4:2 E4:6 | "
     "D5:4 D5:2 F5:3 D5:1 B4:2 | C5:6 E5:4 R:2 | C5:3 G4:1 E4:2 G4:3 F4:1 D4:2 | C4:6~ C4:4 R:2"},
    {"schneefloeckchen", "Schneefl\xC3\xB6""ckchen, Wei\xC3\x9Fr\xC3\xB6""ckchen", "Schneefl\xC3\xB6""ckchen, Wei\xC3\x9Fr\xC3\xB6""ckchen",
     120, 12, 4,
     "E4:2 F4:2 | G4:4 G4:4 A4:4 | D4:4 D4:4 D4:2 E4:2 | F4:4 F4:4 G4:4 | E4:8 E4:2 F4:2 | "
     "G4:4 G4:4 C5:4 | B4:4 A4:4 G4:2 F4:2 | E4:4 F4:4 D4:4 | C4:8"},
    {"wer-hat-die-schoensten-schaefchen", "Wer hat die sch\xC3\xB6nsten Sch\xC3\xA4""fchen",
     "Wer hat die sch\xC3\xB6nsten Sch\xC3\xA4""fchen", 100, 16, 4,
     "F4:4 | C5:4 C5:4 Bb4:4 Bb4:4 | A4:8 G4:4 C5:4 | C5:4 F4:4 Bb4:4 A4:4 | G4:8 R:4 G4:4 | "
     "Bb4:4 Bb4:4 A4:4 A4:4 | D5:8 C5:4 Bb4:4 | A4:4 A4:4 G4:4 G4:4 | F4:8 R:4"},
    {"haensel-und-gretel", "H\xC3\xA4nsel und Gretel", "H\xC3\xA4nsel und Gretel", 120, 16, 0,
     "A4:8 F#4:4 G4:4 | A4:8 F#4:4 D4:4 | E4:4 E4:4 E4:4 F#4:4 | D4:12 R:4 | "
     "A4:8 F#4:4 G4:4 | A4:8 F#4:4 D4:4 | E4:4 E4:4 E4:4 F#4:4 | D4:8 R:4 D4:4 | "
     "E4:4 E4:4 E4:4 F#4:4 | G4:8 E4:4 E4:4 | F#4:4 F#4:4 F#4:4 G4:4 | A4:12 R:4 | "
     "A4:8 F#4:4 G4:4 | A4:8 F#4:4 D4:4 | E4:4 E4:4 E4:4 F#4:4 | D4:12 R:4"},
    {"twinkle-twinkle", "Twinkle, Twinkle, Little Star", "Twinkle, Twinkle, Little Star", 100, 16, 0,
     "C4:4 C4:4 G4:4 G4:4 | A4:4 A4:4 G4:8 | F4:4 F4:4 E4:4 E4:4 | D4:4 D4:4 C4:8 | "
     "G4:4 G4:4 F4:4 F4:4 | E4:4 E4:4 D4:8 | G4:4 G4:4 F4:4 F4:4 | E4:4 E4:4 D4:8 | "
     "C4:4 C4:4 G4:4 G4:4 | A4:4 A4:4 G4:8 | F4:4 F4:4 E4:4 E4:4 | D4:4 D4:4 C4:8"},
    {"frere-jacques", "Bruder Jakob", "Fr\xC3\xA8re Jacques", 108, 16, 0,
     "F4:4 G4:4 A4:4 F4:4 | F4:4 G4:4 A4:4 F4:4 | A4:4 Bb4:4 C5:8 | A4:4 Bb4:4 C5:8 | "
     "C5:2 D5:2 C5:2 Bb4:2 A4:4 F4:4 | C5:2 D5:2 C5:2 Bb4:2 A4:4 F4:4 | F4:4 C4:4 F4:8 | F4:4 C4:4 F4:8"},
    {"row-your-boat", "Row, Row, Row Your Boat", "Row, Row, Row Your Boat", 96, 12, 0,
     "C4:6 C4:6 | C4:4 D4:2 E4:6 | E4:4 D4:2 E4:4 F4:2 | G4:12 | C5:2 C5:2 C5:2 G4:2 G4:2 G4:2 | "
     "E4:2 E4:2 E4:2 C4:2 C4:2 C4:2 | G4:4 F4:2 E4:4 D4:2 | C4:12"},
    {"london-bridge", "London Bridge", "London Bridge", 108, 8, 0,
     "C5:3 D5:1 C5:2 Bb4:2 | A4:2 Bb4:2 C5:4 | G4:2 A4:2 Bb4:4 | A4:2 Bb4:2 C5:4 | "
     "C5:3 D5:1 C5:2 Bb4:2 | A4:2 Bb4:2 C5:4 | G4:4 C5:4 | A4:2 F4:6"},
    {"jingle-bells", "Jingle Bells", "Jingle Bells", 132, 16, 0,
     "B4:4 B4:4 B4:8 | B4:4 B4:4 B4:8 | B4:4 D5:4 G4:6 A4:2 | B4:12 R:4 | C5:4 C5:4 C5:6 C5:2 | "
     "C5:4 B4:4 B4:4 B4:2 B4:2 | B4:4 A4:4 A4:4 B4:4 | A4:8 D5:4 R:4 | "
     "B4:4 B4:4 B4:8 | B4:4 B4:4 B4:8 | B4:4 D5:4 G4:6 A4:2 | B4:12 R:4 | C5:4 C5:4 C5:6 C5:2 | "
     "C5:4 B4:4 B4:4 B4:2 B4:2 | D5:4 D5:4 C5:4 A4:4 | G4:12 R:4"},
    {"ode-to-joy", "Ode an die Freude", "Ode to Joy", 112, 16, 0,
     "B4:4 B4:4 C5:4 D5:4 | D5:4 C5:4 B4:4 A4:4 | G4:4 G4:4 A4:4 B4:4 | B4:6 A4:2 A4:8 | "
     "B4:4 B4:4 C5:4 D5:4 | D5:4 C5:4 B4:4 A4:4 | G4:4 G4:4 A4:4 B4:4 | A4:6 G4:2 G4:8 | "
     "A4:4 A4:4 B4:4 G4:4 | A4:4 B4:2 C5:2 B4:4 G4:4 | A4:4 B4:2 C5:2 B4:4 A4:4 | G4:4 A4:4 D4:4 B4:4~ | "
     "B4:4 B4:4 C5:4 D5:4 | D5:4 C5:4 B4:4 A4:4 | G4:4 G4:4 A4:4 B4:4 | A4:6 G4:2 G4:8"},
    {"happy-birthday", "Zum Geburtstag viel Gl\xC3\xBC""ck", "Happy Birthday", 96, 12, 4,
     "D4:3 D4:1 | E4:4 D4:4 G4:4 | F#4:8 D4:3 D4:1 | E4:4 D4:4 A4:4 | G4:8 D4:3 D4:1 | "
     "D5:4 B4:4 G4:4 | F#4:4 E4:4 C5:3 C5:1 | B4:4 G4:4 A4:4 | G4:8"},
    {"mulberry-bush", "Here We Go Round the Mulberry Bush", "Here We Go Round the Mulberry Bush", 100, 12, 0,
     "G4:2 G4:2 G4:2 G4:4 B4:2 | D5:4 B4:2 G4:4 G4:2 | A4:4 A4:2 A4:4 G4:2 | F#4:4 E4:2 D4:6 | "
     "G4:2 G4:2 G4:2 G4:4 B4:2 | D5:4 B4:2 G4:4 G4:2 | A4:4 A4:2 D4:4 D4:2 | G4:6 G4:6"},
};
static const int MUSIC_SONG_COUNT = sizeof(MUSIC_SONGS) / sizeof(MUSIC_SONGS[0]);

// Added to the AI prompt. The test checks that every id above is named here.
static const char* const MUSIC_PROMPT_RULE =
    "MUSIK: Setze 'model' auf 'music:<id>' NUR wenn der Nutzer eines dieser Lieder hoeren, spielen oder seine Noten "
    "sehen will, mit genau dieser id: alle-meine-entchen, haenschen-klein, fuchs-du-hast-die-gans, kommt-ein-vogel, "
    "ein-maennlein, alle-voegel, kuckuck-und-esel, kuckuck-ruft, backe-backe-kuchen, schlaf-kindlein, "
    "guten-abend-gut-nacht, haeschen-in-der-grube, summ-summ-summ, o-tannenbaum, stille-nacht, schneefloeckchen, "
    "wer-hat-die-schoensten-schaefchen, haensel-und-gretel, twinkle-twinkle, frere-jacques, row-your-boat, "
    "london-bridge, jingle-bells, ode-to-joy, happy-birthday, mulberry-bush "
    "(z.B. 'Spiel Alle meine Entchen' ergibt 'music:alle-meine-entchen', 'Wie geht Bruder Jakob?' ergibt "
    "'music:frere-jacques', 'Play Silent Night' ergibt 'music:stille-nacht'). 'Spiel die Ode an die Freude', 'Spiel "
    "Beethovens Neunte', 'Spiel Beethovens Sinfonie' und 'Play Beethovens symphony' ergeben 'music:ode-to-joy', auch "
    "ohne genaue Nummer der Sinfonie; dann sagt answer, dass die Melodie der Ode an die Freude aus der 9. Sinfonie "
    "klingt, nicht die ganze Sinfonie. Bei Fragen ueber ein Lied ('Wer schrieb Stille Nacht?') bleibt model leer. "
    "Zitro hat keine Aufnahmen und kann nicht singen: es spielt nur diese Melodien. Will der Nutzer andere Musik "
    "hoeren ('Spiel Beethovens Fuenfte', 'Play Mozart', 'Sing mir ein Lied'), bleibt model leer und answer sagt "
    "ehrlich, dass du das nicht spielen kannst, und nennt als Beispiel ein Lied aus der Liste. Schreibe ohne 'music:' "
    "nie 'Ich spiele', 'Ich singe', 'Playing' oder Aehnliches. Auch hier gilt: answer in der Sprache der Frage und in "
    "der Ich-Form; auf 'Play Mozart' antwortest du englisch, z.B. 'I can\'t play Mozart, but I can play Twinkle "
    "Twinkle.' Nenne nie selbst Noten oder Melodien: sie kommen aus einer geprueften Tabelle. Wenn model mit 'music:' "
    "gesetzt ist, ist is_audio false und answer nennt nur kurz das Lied. Fuelle image_query auch dann wie immer.";

// Only an exact id counts, in any case: a near match would play the wrong song under its name.
inline const MusicSong* musicFindSong(const char* id) {
  if (!id) return nullptr;
  for (int i = 0; i < MUSIC_SONG_COUNT; i++) {
    const char* a = MUSIC_SONGS[i].id;
    const char* b = id;
    while (*a && (*a == *b || (*b >= 'A' && *b <= 'Z' && *a == *b + 32))) {
      a++;
      b++;
    }
    if (!*a && !*b) return &MUSIC_SONGS[i];
  }
  return nullptr;
}
