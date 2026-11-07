// Melodies Zitro can play without WiFi: music:<title>. Written by hand, compared note by note against published
// scores by tests/music/run.sh, and all in the public domain: see docs/features/music.md.
// The AI only names a title, and the stick decides whether it knows the song. Notes never come from the AI.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "name_match.h"

struct MusicSong {
  const char* id;
  const char* nameDe;
  const char* nameEn;
  uint8_t beatsPerMinute;  // Quarter notes
  uint8_t barTicks;        // Length of a bar in sixteenths
  uint8_t pickupTicks;     // Lead-in before the first bar line; the last bar makes up for it
  const char* notes;       // See music_core.h
  // Other names it is found by, ';'-separated and written normalised (see name_match.h), or nullptr.
  // Only names of this song, in German and English: a title that could mean several pieces gets none.
  const char* aliases;
};

static const MusicSong MUSIC_SONGS[] = {
    {"alle-meine-entchen", "Alle meine Entchen", "Alle meine Entchen", 104, 16, 0,
     "C4:2 D4:2 E4:2 F4:2 G4:4 G4:4 | A4:2 A4:2 A4:2 A4:2 G4:8 | A4:2 A4:2 A4:2 A4:2 G4:8 | "
     "F4:2 F4:2 F4:2 F4:2 E4:4 E4:4 | D4:2 D4:2 D4:2 D4:2 C4:8", nullptr},
    {"haenschen-klein", "H\xC3\xA4nschen klein", "H\xC3\xA4nschen klein", 100, 8, 0,
     "C5:2 A4:2 A4:4 | Bb4:2 G4:2 G4:4 | F4:2 G4:2 A4:2 Bb4:2 | C5:2 C5:2 C5:4 | "
     "C5:2 A4:2 A4:4 | Bb4:2 G4:2 G4:4 | F4:2 A4:2 C5:2 C5:2 | F4:4 R:4 | "
     "G4:2 G4:2 G4:2 G4:2 | G4:2 A4:2 Bb4:4 | A4:2 A4:2 A4:2 A4:2 | A4:2 Bb4:2 C5:4 | "
     "C5:2 A4:2 A4:4 | Bb4:2 G4:2 G4:4 | F4:2 A4:2 C5:2 C5:2 | F4:8", nullptr},
    {"fuchs-du-hast-die-gans", "Fuchs, du hast die Gans gestohlen", "Fuchs, du hast die Gans gestohlen", 120, 16, 0,
     "D4:4 E4:4 F#4:4 G4:4 | A4:4 A4:4 A4:4 A4:4 | B4:4 G4:4 D5:4 B4:4 | A4:16 | B4:4 G4:4 D5:4 B4:4 | A4:16 | "
     "A4:4 G4:4 G4:4 G4:4 | G4:4 F#4:4 F#4:4 F#4:4 | F#4:4 E4:4 F#4:4 E4:4 | D4:4 F#4:4 A4:8 | "
     "A4:4 G4:4 G4:4 G4:4 | G4:4 F#4:4 F#4:4 F#4:4 | F#4:4 E4:4 F#4:4 E4:4 | D4:16", nullptr},
    {"kommt-ein-vogel", "Kommt ein Vogel geflogen", "Kommt ein Vogel geflogen", 120, 12, 4,
     "A4:2 Bb4:2 | C5:4 A4:4 A4:4 | A4:4 G4:4 G4:2 A4:2 | Bb4:4 G4:4 G4:2 D5:2 | C5:4 R:4 A4:2 Bb4:2 | "
     "C5:4 A4:4 A4:4 | A4:4 G4:4 G4:2 A4:2 | Bb4:4 E4:4 E4:4 | F4:4 R:4", nullptr},
    {"ein-maennlein", "Ein M\xC3\xA4nnlein steht im Walde", "Ein M\xC3\xA4nnlein steht im Walde", 132, 16, 4,
     "A3:4 | D4:4 E4:4 F#4:4 G4:4 | A4:8 B4:4 G4:4 | F#4:8 E4:8 | D4:8 R:4 A3:4 | "
     "D4:4 E4:4 F#4:4 G4:4 | A4:8 B4:4 G4:4 | F#4:8 E4:8 | D4:8 R:8 | "
     "A4:6 G4:2 F#4:4 A4:4 | G4:4 F#4:4 E4:8 | A4:6 G4:2 F#4:4 A4:4 | G4:4 F#4:4 E4:8 | "
     "D4:4 E4:4 F#4:4 G4:4 | A4:8 B4:4 G4:4 | F#4:8 E4:8 | D4:8 R:4", nullptr},
    {"alle-voegel", "Alle V\xC3\xB6gel sind schon da", "Alle V\xC3\xB6gel sind schon da", 120, 16, 0,
     "D4:6 F#4:2 A4:4 D5:4 | B4:4 D5:2 B4:2 A4:8 | G4:6 A4:2 F#4:4 D4:4 | E4:8 D4:4 R:4 | "
     "A4:4 A4:4 G4:4 G4:4 | F#4:4 A4:2 F#4:2 E4:8 | A4:4 A4:4 G4:4 G4:4 | F#4:4 A4:2 F#4:2 E4:8 | "
     "D4:6 F#4:2 A4:4 D5:4 | B4:4 D5:2 B4:2 A4:8 | G4:6 A4:2 F#4:4 D4:4 | E4:8 D4:4 R:4", nullptr},
    {"kuckuck-und-esel", "Der Kuckuck und der Esel", "Der Kuckuck und der Esel", 132, 16, 4,
     "C5:4 | A4:4 C5:4 A4:4 C5:4 | Bb4:4 Bb4:4 R:4 Bb4:4 | G4:4 Bb4:4 G4:4 Bb4:4 | A4:8 R:4 C5:2 Bb4:2 | "
     "A4:4 A4:4 A4:4 A4:4 | Bb4:4 Bb4:4 R:4 Bb4:2 A4:2 | G4:4 G4:4 G4:4 G4:4 | A4:4 A4:4 R:4 F4:4 | "
     "F4:4 G4:4 A4:4 Bb4:4 | C5:6 D5:2 C5:4 Bb4:4 | A4:4 A4:4 G4:4 G4:4 | F4:8 R:4", nullptr},
    {"kuckuck-ruft", "Kuckuck, Kuckuck, ruft's aus dem Wald", "Kuckuck, Kuckuck, ruft's aus dem Wald", 132, 12, 0,
     "D5:4 B4:4 R:4 | D5:4 B4:4 R:4 | A4:4 G4:4 A4:4 | G4:12 | A4:4 A4:4 B4:4 | C5:8 A4:4 | "
     "B4:4 B4:4 C5:4 | D5:8 B4:4 | D5:8 B4:4 | D5:8 B4:4 | C5:4 B4:4 A4:4 | G4:12", nullptr},
    {"backe-backe-kuchen", "Backe, backe Kuchen", "Backe, backe Kuchen", 132, 16, 0,
     "A4:4 A4:4 B4:4 B4:4 | A4:8 F#4:4 D4:4 | A4:4 A4:4 B4:4 B4:4 | A4:8 F#4:8 | "
     "A4:4 A4:4 B4:4 B4:4 | A4:4 A4:4 F#4:4 D4:4 | A4:4 A4:4 B4:4 B4:4 | A4:4 A4:4 F#4:4 F#4:4 | "
     "A4:2 A4:2 A4:4 F#4:8 | A4:2 A4:2 A4:4 F#4:8 | A4:4 A4:4 F#4:8 | "
     "A4:4 A4:4 B4:4 B4:4 | A4:4 A4:4 F#4:8 | D5:8 A4:4 G4:4 | F#4:4 E4:4 D4:8", nullptr},
    {"schlaf-kindlein", "Schlaf, Kindlein, schlaf", "Schlaf, Kindlein, schlaf", 72, 8, 0,
     "A4:4 G4:2 G4:2 | F4:4 R:2 F4:1 A4:1 | C5:2 C5:2 Bb4:2 Bb4:2 | A4:4 R:2 A4:2 | Bb4:2 Bb4:2 G4:2 G4:2 | "
     "C5:2 C5:2 A4:2 A4:2 | Bb4:2 Bb4:2 G4:2 G4:2 | C5:2 C5:2 A4:4 | Bb4:4 G4:2 G4:2 | F4:4 R:4",
     "sleep baby sleep"},
    {"guten-abend-gut-nacht", "Guten Abend, gut' Nacht", "Brahms' Lullaby", 76, 12, 4,
     "G4:2 G4:2 | Bb4:6 G4:2 G4:4 | Bb4:4 R:4 G4:2 Bb4:2 | Eb5:4 D5:6 C5:2 | C5:4 Bb4:4 F4:2 G4:2 | "
     "Ab4:4 F4:4 F4:2 G4:2 | Ab4:4 R:4 F4:2 Ab4:2 | D5:2 C5:2 Bb4:4 D5:4 | Eb5:4 R:4 Eb4:2 Eb4:2 | "
     "Eb5:8 C5:2 Ab4:2 | Bb4:8 G4:2 Eb4:2 | Ab4:4 Bb4:4 C5:4 | Bb4:8 Eb4:2 Eb4:2 | "
     "Eb5:8 C5:2 Ab4:2 | Bb4:8 G4:2 Eb4:2 | Ab4:4 G4:4 F4:4 | Eb4:8",
     "brahms wiegenlied;wiegenlied von brahms;lullaby and goodnight;lullaby and good night"},
    {"haeschen-in-der-grube", "H\xC3\xA4schen in der Grube", "H\xC3\xA4schen in der Grube", 100, 8, 0,
     "D4:2 E4:2 F#4:2 G4:2 | A4:4 A4:4 | B4:2 G4:2 D5:2 B4:2 | A4:4 R:4 | B4:2 G4:2 D5:2 B4:2 | A4:4 R:4 | "
     "A4:2 G4:2 G4:2 G4:2 | G4:2 F#4:2 F#4:4 | F#4:2 E4:2 F#4:2 E4:2 | D4:2 F#4:2 A4:4 | "
     "A4:2 A4:2 D5:4 | A4:2 A4:2 D5:4 | A4:4 A4:4 | D4:4 R:4", nullptr},
    {"summ-summ-summ", "Summ, summ, summ", "Summ, summ, summ", 104, 8, 0,
     "D5:4 C5:4 | B4:4 R:4 | A4:2 B4:2 C5:2 A4:2 | G4:4 R:4 | B4:2 C5:2 D5:2 B4:2 | A4:2 B4:2 C5:2 A4:2 | "
     "B4:2 C5:2 D5:2 B4:2 | A4:2 B4:2 C5:2 A4:2 | D5:4 C5:4 | B4:4 R:4 | A4:2 B4:2 C5:2 A4:2 | G4:4 R:4", nullptr},
    {"o-tannenbaum", "O Tannenbaum", "O Christmas Tree", 88, 12, 2,
     "D4:2 | G4:3 G4:1 G4:4 A4:4 | B4:3 B4:1 B4:6 B4:2 | A4:2 B4:2 C5:4 F#4:4 | A4:4 G4:4 R:2 "
     "D5:2 | D5:2 B4:2 E5:6 D5:2 | D5:2 C5:2 C5:6 C5:2 | C5:2 A4:2 D5:6 C5:2 | C5:2 B4:2 B4:4 R:2 "
     "D4:2 | G4:3 G4:1 G4:4 A4:4 | B4:3 B4:1 B4:6 B4:2 | A4:2 B4:2 C5:4 F#4:4 | A4:4 G4:4 R:2",
     "oh tannenbaum;oh christmas tree"},
    {"stille-nacht", "Stille Nacht", "Silent Night", 66, 12, 0,
     "G4:3 A4:1 G4:2 E4:6 | G4:3 A4:1 G4:2 E4:6 | D5:4 D5:2 B4:6 | C5:4 C5:2 G4:6 | "
     "A4:4 A4:2 C5:3 B4:1 A4:2 | G4:3 A4:1 G4:2 E4:6 | A4:4 A4:2 C5:3 B4:1 A4:2 | G4:3 A4:1 G4:2 E4:6 | "
     "D5:4 D5:2 F5:3 D5:1 B4:2 | C5:6 E5:4 R:2 | C5:3 G4:1 E4:2 G4:3 F4:1 D4:2 | C4:6~ C4:4 R:2",
     "silent night;stille nacht heilige nacht;silent night holy night"},
    {"schneefloeckchen", "Schneefl\xC3\xB6""ckchen, Wei\xC3\x9Fr\xC3\xB6""ckchen", "Schneefl\xC3\xB6""ckchen, Wei\xC3\x9Fr\xC3\xB6""ckchen",
     120, 12, 4,
     "E4:2 F4:2 | G4:4 G4:4 A4:4 | D4:4 D4:4 D4:2 E4:2 | F4:4 F4:4 G4:4 | E4:8 E4:2 F4:2 | "
     "G4:4 G4:4 C5:4 | B4:4 A4:4 G4:2 F4:2 | E4:4 F4:4 D4:4 | C4:8", nullptr},
    {"wer-hat-die-schoensten-schaefchen", "Wer hat die sch\xC3\xB6nsten Sch\xC3\xA4""fchen",
     "Wer hat die sch\xC3\xB6nsten Sch\xC3\xA4""fchen", 100, 16, 4,
     "F4:4 | C5:4 C5:4 Bb4:4 Bb4:4 | A4:8 G4:4 C5:4 | C5:4 F4:4 Bb4:4 A4:4 | G4:8 R:4 G4:4 | "
     "Bb4:4 Bb4:4 A4:4 A4:4 | D5:8 C5:4 Bb4:4 | A4:4 A4:4 G4:4 G4:4 | F4:8 R:4", nullptr},
    {"haensel-und-gretel", "H\xC3\xA4nsel und Gretel", "H\xC3\xA4nsel und Gretel", 120, 16, 0,
     "A4:8 F#4:4 G4:4 | A4:8 F#4:4 D4:4 | E4:4 E4:4 E4:4 F#4:4 | D4:12 R:4 | "
     "A4:8 F#4:4 G4:4 | A4:8 F#4:4 D4:4 | E4:4 E4:4 E4:4 F#4:4 | D4:8 R:4 D4:4 | "
     "E4:4 E4:4 E4:4 F#4:4 | G4:8 E4:4 E4:4 | F#4:4 F#4:4 F#4:4 G4:4 | A4:12 R:4 | "
     "A4:8 F#4:4 G4:4 | A4:8 F#4:4 D4:4 | E4:4 E4:4 E4:4 F#4:4 | D4:12 R:4", nullptr},
    {"twinkle-twinkle", "Twinkle, Twinkle, Little Star", "Twinkle, Twinkle, Little Star", 100, 16, 0,
     "C4:4 C4:4 G4:4 G4:4 | A4:4 A4:4 G4:8 | F4:4 F4:4 E4:4 E4:4 | D4:4 D4:4 C4:8 | "
     "G4:4 G4:4 F4:4 F4:4 | E4:4 E4:4 D4:8 | G4:4 G4:4 F4:4 F4:4 | E4:4 E4:4 D4:8 | "
     "C4:4 C4:4 G4:4 G4:4 | A4:4 A4:4 G4:8 | F4:4 F4:4 E4:4 E4:4 | D4:4 D4:4 C4:8",
     "twinkle twinkle little star;twinkle twinkle"},
    {"frere-jacques", "Bruder Jakob", "Fr\xC3\xA8re Jacques", 108, 16, 0,
     "F4:4 G4:4 A4:4 F4:4 | F4:4 G4:4 A4:4 F4:4 | A4:4 Bb4:4 C5:8 | A4:4 Bb4:4 C5:8 | "
     "C5:2 D5:2 C5:2 Bb4:2 A4:4 F4:4 | C5:2 D5:2 C5:2 Bb4:2 A4:4 F4:4 | F4:4 C4:4 F4:8 | F4:4 C4:4 F4:8",
     "bruder jakob;brother john;are you sleeping;are you sleeping brother john"},
    {"row-your-boat", "Row, Row, Row Your Boat", "Row, Row, Row Your Boat", 96, 12, 0,
     "C4:6 C4:6 | C4:4 D4:2 E4:6 | E4:4 D4:2 E4:4 F4:2 | G4:12 | C5:2 C5:2 C5:2 G4:2 G4:2 G4:2 | "
     "E4:2 E4:2 E4:2 C4:2 C4:2 C4:2 | G4:4 F4:2 E4:4 D4:2 | C4:12",
     "row your boat"},
    {"london-bridge", "London Bridge", "London Bridge", 108, 8, 0,
     "C5:3 D5:1 C5:2 Bb4:2 | A4:2 Bb4:2 C5:4 | G4:2 A4:2 Bb4:4 | A4:2 Bb4:2 C5:4 | "
     "C5:3 D5:1 C5:2 Bb4:2 | A4:2 Bb4:2 C5:4 | G4:4 C5:4 | A4:2 F4:6",
     "london bridge is falling down"},
    {"jingle-bells", "Jingle Bells", "Jingle Bells", 132, 16, 0,
     "B4:4 B4:4 B4:8 | B4:4 B4:4 B4:8 | B4:4 D5:4 G4:6 A4:2 | B4:12 R:4 | C5:4 C5:4 C5:6 C5:2 | "
     "C5:4 B4:4 B4:4 B4:2 B4:2 | B4:4 A4:4 A4:4 B4:4 | A4:8 D5:4 R:4 | "
     "B4:4 B4:4 B4:8 | B4:4 B4:4 B4:8 | B4:4 D5:4 G4:6 A4:2 | B4:12 R:4 | C5:4 C5:4 C5:6 C5:2 | "
     "C5:4 B4:4 B4:4 B4:2 B4:2 | D5:4 D5:4 C5:4 A4:4 | G4:12 R:4", nullptr},
    {"ode-to-joy", "Ode an die Freude", "Ode to Joy", 112, 16, 0,
     "B4:4 B4:4 C5:4 D5:4 | D5:4 C5:4 B4:4 A4:4 | G4:4 G4:4 A4:4 B4:4 | B4:6 A4:2 A4:8 | "
     "B4:4 B4:4 C5:4 D5:4 | D5:4 C5:4 B4:4 A4:4 | G4:4 G4:4 A4:4 B4:4 | A4:6 G4:2 G4:8 | "
     "A4:4 A4:4 B4:4 G4:4 | A4:4 B4:2 C5:2 B4:4 G4:4 | A4:4 B4:2 C5:2 B4:4 A4:4 | G4:4 A4:4 D4:4 B4:4~ | "
     "B4:4 B4:4 C5:4 D5:4 | D5:4 C5:4 B4:4 A4:4 | G4:4 G4:4 A4:4 B4:4 | A4:6 G4:2 G4:8",
     "beethovens sinfonie;beethovens symphony;beethoven symphony;beethovens neunte;beethovens ninth;"
     "freude schoener goetterfunken;beethoven sinfonie;beethovens 9 sinfonie;beethovens ninth symphony;"
     "beethovens 9th symphony"},
    {"happy-birthday", "Zum Geburtstag viel Gl\xC3\xBC""ck", "Happy Birthday", 96, 12, 4,
     "D4:3 D4:1 | E4:4 D4:4 G4:4 | F#4:8 D4:3 D4:1 | E4:4 D4:4 A4:4 | G4:8 D4:3 D4:1 | "
     "D5:4 B4:4 G4:4 | F#4:4 E4:4 C5:3 C5:1 | B4:4 G4:4 A4:4 | G4:8",
     "zum geburtstag viel glueck;happy birthday to you"},
    {"mulberry-bush", "Here We Go Round the Mulberry Bush", "Here We Go Round the Mulberry Bush", 100, 12, 0,
     "G4:2 G4:2 G4:2 G4:4 B4:2 | D5:4 B4:2 G4:4 G4:2 | A4:4 A4:2 A4:4 G4:2 | F#4:4 E4:2 D4:6 | "
     "G4:2 G4:2 G4:2 G4:4 B4:2 | D5:4 B4:2 G4:4 G4:2 | A4:4 A4:2 D4:4 D4:2 | G4:6 G4:6",
     "mulberry bush"},
};
static const int MUSIC_SONG_COUNT = sizeof(MUSIC_SONGS) / sizeof(MUSIC_SONGS[0]);

// Added to the AI prompt. The AI gives only a title; the stick looks it up, so the rule lists no songs.
static const char* const MUSIC_PROMPT_RULE =
    "MUSIK: Will der Nutzer Musik hoeren, spielen, singen lassen oder ihre Noten sehen, setze 'model' auf "
    "'music:<Titel>', mit dem bekanntesten Titel des Stuecks in seiner Originalsprache und ohne Komponist "
    "(z.B. 'Spiel Alle meine Entchen' ergibt 'music:Alle meine Entchen', 'Play Silent Night' ergibt "
    "'music:Silent Night', 'Spiel Beethovens Sinfonie' ergibt 'music:Ode an die Freude', 'Play Mozart' ergibt "
    "'music:Mozart', 'Sing mir ein Lied' ergibt 'music:ein Lied'). Dann ist is_audio false und answer ist nur der "
    "Titel, ohne Versprechen: Zitro prueft selbst, ob es das Lied kennt. Schreibe also nie 'Ich spiele', "
    "'Ich singe', 'Playing' oder Aehnliches. Bei Fragen ueber ein Lied ('Wer schrieb Stille Nacht?') bleibt model "
    "leer. Nenne nie selbst Noten oder Melodien. Schreibe answer in der Sprache der Frage: auf eine englische Frage "
    "antwortest du englisch, auf eine deutsche deutsch. Fuelle image_query auch dann wie immer.";

// Two ways to find a song: its exact id in any case, so old music:<id> references keep working, or a title
// equal to its German or English name or an alias once both are normalised. Never a near match: that would
// play the wrong song under its name. A title that fits two songs finds neither.
inline const MusicSong* musicFindBuiltIn(const char* title) {
  if (!title) return nullptr;
  for (int i = 0; i < MUSIC_SONG_COUNT; i++) {
    const char* a = MUSIC_SONGS[i].id;
    const char* b = title;
    while (*a && (*a == *b || (*b >= 'A' && *b <= 'Z' && *a == *b + 32))) {
      a++;
      b++;
    }
    if (!*a && !*b) return &MUSIC_SONGS[i];
  }
  char wanted[128];
  if (!eduNormaliseName(title, wanted, sizeof(wanted), true)) return nullptr;
  const size_t wantedLength = strlen(wanted);
  const MusicSong* found = nullptr;
  char name[128];
  for (int i = 0; i < MUSIC_SONG_COUNT; i++) {
    const MusicSong& s = MUSIC_SONGS[i];
    bool match = (eduNormaliseName(s.nameDe, name, sizeof(name), true) && !strcmp(name, wanted)) ||
                 (eduNormaliseName(s.nameEn, name, sizeof(name), true) && !strcmp(name, wanted));
    for (const char* alias = s.aliases; alias && *alias && !match;) {
      const char* semi = strchr(alias, ';');
      const size_t length = semi ? (size_t)(semi - alias) : strlen(alias);
      match = length == wantedLength && !strncmp(alias, wanted, length);
      alias = semi ? semi + 1 : alias + length;
    }
    if (!match) continue;
    if (found) return nullptr;
    found = &s;
  }
  return found;
}

enum MusicLookup { MUSIC_LOOKUP_FOUND, MUSIC_LOOKUP_UNKNOWN, MUSIC_LOOKUP_OFFLINE };

// A built-in song to try instead, picked by seed. Each is named in ASCII in its language.
inline const MusicSong* musicSuggestion(bool german, unsigned seed) {
  static const char* const GERMAN[] = {"alle-meine-entchen", "frere-jacques", "summ-summ-summ", "backe-backe-kuchen"};
  static const char* const ENGLISH[] = {"twinkle-twinkle", "row-your-boat", "london-bridge", "mulberry-bush"};
  const char* id = (german ? GERMAN : ENGLISH)[seed % 4];
  for (int i = 0; i < MUSIC_SONG_COUNT; i++) {
    if (!strcmp(MUSIC_SONGS[i].id, id)) return &MUSIC_SONGS[i];
  }
  return nullptr;
}

// The sentence that replaces the AI's answer when no song plays. "I don't know it" would be untrue when the
// song list can't be reached, so that gets its own sentence. MUSIC_LOOKUP_FOUND needs none: out is empty.
inline void musicMissAnswer(MusicLookup reason, bool german, unsigned seed, char* out, size_t capacity) {
  if (!out || capacity == 0) return;
  out[0] = 0;
  const MusicSong* song = musicSuggestion(german, seed);
  if (reason == MUSIC_LOOKUP_FOUND || !song) return;
  const char* format;
  if (german) {
    format = reason == MUSIC_LOOKUP_OFFLINE ? "Ich erreiche gerade meine Liederliste nicht. Probier mal %s!"
                                            : "Dieses Lied kenne ich nicht. Probier mal %s!";
  } else {
    format = reason == MUSIC_LOOKUP_OFFLINE ? "I can't reach my song list right now. Try %s!"
                                            : "I don't know that song. Try %s!";
  }
  snprintf(out, capacity, format, german ? song->nameDe : song->nameEn);
}
