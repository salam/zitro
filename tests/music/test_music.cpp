// Host tests for the music band. Build and run: tests/music/run.sh
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include "../../sketch/sketch_jan12a/music_core.h"
#include "../../sketch/sketch_jan12a/music_songs.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, e) CHECK(fabs((a) - (b)) <= (e))

static MusicNote notes[MUSIC_MAX_NOTES];

static void testParser() {
  int total = 0;
  CHECK(musicParseNotes("C4:4 F#4:2 | Bb4:2 R:4 G4:4~ G4:4", notes, MUSIC_MAX_NOTES, total) == 6);
  CHECK(total == 20);
  CHECK(notes[0].midi == 60 && notes[0].ticks == 4 && notes[0].staffStep == -2 && notes[0].startTick == 0);
  CHECK(notes[1].midi == 66 && notes[1].accidental == 1 && notes[1].staffStep == 1 && notes[1].startTick == 4);
  CHECK(notes[2].midi == 70 && notes[2].accidental == -1 && notes[2].staffStep == 4);
  CHECK(notes[3].midi == 0 && notes[3].ticks == 4);
  CHECK(notes[4].tied && !notes[5].tied && notes[4].midi == notes[5].midi);
  CHECK(notes[5].startTick == 16);

  const char* bad[] = {"", "   ", "|", "H4:4", "c4:4", "C:4", "C4", "C4:", "C4:5", "C4:0", "C4:99999", "C4:-4",
                       "C1:4", "C7:4", "C#b4:4", "R:3~ C4:4", "R:5", "C4:4~", "C4:4~ D4:4", "C4:4~ R:4",
                       "C4:4,D4:4", "C4:4 \xC3\xA4", "C4:4x", "Z", ":", "C4:4 :4"};
  for (const char* s : bad) {
    total = 123;
    if (musicParseNotes(s, notes, MUSIC_MAX_NOTES, total) != -1) {
      fprintf(stderr, "  accepted '%s'\n", s);
      CHECK(false);
    }
    CHECK(total == 0);
  }
  CHECK(musicParseNotes(nullptr, notes, MUSIC_MAX_NOTES, total) == -1);
  CHECK(musicParseNotes("C4:4", nullptr, MUSIC_MAX_NOTES, total) == -1);
  CHECK(musicParseNotes("C4:4", notes, 0, total) == -1);

  // Never writes past the capacity
  std::string many;
  for (int i = 0; i < MUSIC_MAX_NOTES + 50; i++) many += "C4:1 ";
  MusicNote small[3];
  CHECK(musicParseNotes(many.c_str(), small, 3, total) == -1);
  CHECK(musicParseNotes(many.c_str(), notes, MUSIC_MAX_NOTES, total) == -1);
}

static void testPitch() {
  NEAR(musicFrequencyHz(69), 440.0f, 0.01f);   // A4
  NEAR(musicFrequencyHz(60), 261.63f, 0.05f);  // C4
  NEAR(musicFrequencyHz(72), 523.25f, 0.05f);  // C5
  NEAR(musicFrequencyHz(66), 369.99f, 0.05f);  // F#4
  NEAR(musicFrequencyHz(70), 466.16f, 0.05f);  // Bb4
}

static void testNames() {
  int total = 0;
  CHECK(musicParseNotes("B4:4 Bb4:4 F#4:4 Eb4:4 Ab4:4 C4:4 R:4", notes, MUSIC_MAX_NOTES, total) == 7);
  CHECK(!strcmp(musicNoteName(notes[0], true), "H"));
  CHECK(!strcmp(musicNoteName(notes[0], false), "B"));
  CHECK(!strcmp(musicNoteName(notes[1], true), "B"));
  CHECK(!strcmp(musicNoteName(notes[1], false), "Bb"));
  CHECK(!strcmp(musicNoteName(notes[2], true), "Fis"));
  CHECK(!strcmp(musicNoteName(notes[2], false), "F#"));
  CHECK(!strcmp(musicNoteName(notes[3], true), "Es"));
  CHECK(!strcmp(musicNoteName(notes[4], true), "As"));
  CHECK(!strcmp(musicNoteName(notes[5], true), "C"));
  CHECK(!strcmp(musicNoteName(notes[6], true), ""));
  MusicNote broken = notes[0];
  broken.letter = 200;
  CHECK(!strcmp(musicNoteName(broken, true), ""));
}

static void testPlayback() {
  int total = 0;
  const int count = musicParseNotes("C4:4 R:4 D4:4~ D4:4 E4:2 F4:2", notes, MUSIC_MAX_NOTES, total);
  CHECK(count == 6);
  const float msPerTick = musicMsPerTick(60);  // A quarter is 1 s
  NEAR(msPerTick, 250.0f, 0.001f);
  MusicPlayback p;
  musicPlaybackReset(p);
  uint8_t midi = 0;
  uint32_t ms = 0;

  // The first step sounds the first note without moving, even with no time passed
  CHECK(musicAdvance(p, notes, count, msPerTick, 0, midi, ms));
  CHECK(midi == 60);
  CHECK(ms == 880);
  CHECK(!musicAdvance(p, notes, count, msPerTick, 0.5f, midi, ms));
  CHECK(p.note == 0);
  NEAR(p.tickInNote, 2.0f, 0.001f);

  // Rest: moves on without sound
  CHECK(!musicAdvance(p, notes, count, msPerTick, 0.5f, midi, ms));
  CHECK(p.note == 1);

  // Tied notes sound once, for both lengths together
  CHECK(musicAdvance(p, notes, count, msPerTick, 1.0f, midi, ms));
  CHECK(p.note == 2 && midi == 62);
  CHECK(ms == 1760);
  CHECK(!musicAdvance(p, notes, count, msPerTick, 1.0f, midi, ms));
  CHECK(p.note == 3);

  // Pause holds the band and stays silent; resuming sounds what's left of the note
  CHECK(musicAdvance(p, notes, count, msPerTick, 1.25f, midi, ms));
  CHECK(p.note == 4 && midi == 64);
  musicTogglePause(p);
  const float held = p.tickInNote;
  CHECK(!musicAdvance(p, notes, count, msPerTick, 30.0f, midi, ms));
  CHECK(p.note == 4 && p.tickInNote == held);
  musicTogglePause(p);
  CHECK(musicAdvance(p, notes, count, msPerTick, 0.0f, midi, ms));
  CHECK(midi == 64 && p.note == 4);
  CHECK(ms == 220);  // 1 of 2 ticks left

  // The end joins the start: the band never ends
  CHECK(musicAdvance(p, notes, count, msPerTick, 0.375f, midi, ms));
  CHECK(p.note == 5 && midi == 65);
  CHECK(musicAdvance(p, notes, count, msPerTick, 0.5f, midi, ms));
  CHECK(p.note == 0 && midi == 60);

  // A long stall jumps to the right note, sounds only that one and stays in time
  musicPlaybackReset(p);
  CHECK(musicAdvance(p, notes, count, msPerTick, 0, midi, ms));
  CHECK(musicAdvance(p, notes, count, msPerTick, 2.25f, midi, ms));
  CHECK(p.note == 2 && midi == 62);
  NEAR(p.tickInNote, 1.0f, 0.001f);
  CHECK(ms == 1540);  // 7 of 8 ticks left

  // Many laps at once
  musicPlaybackReset(p);
  CHECK(musicAdvance(p, notes, count, msPerTick, 0, midi, ms));
  musicAdvance(p, notes, count, msPerTick, 200.0f, midi, ms);
  CHECK(p.note >= 0 && p.note < count);

  // Nothing played for 5 minutes: it pauses, so the stick can sleep
  musicPlaybackReset(p);
  for (int i = 0; i < 7600 && !p.paused; i++) musicAdvance(p, notes, count, msPerTick, 0.04f, midi, ms);
  CHECK(p.paused);
  NEAR(p.playedSeconds, MUSIC_PLAY_CAP_SECONDS, 0.1f);
  musicTogglePause(p);
  CHECK(!p.paused);
  NEAR(p.playedSeconds, 0.0f, 0.001f);

  // Broken input is refused, not read
  musicPlaybackReset(p);
  CHECK(!musicAdvance(p, nullptr, count, msPerTick, 0.1f, midi, ms));
  CHECK(!musicAdvance(p, notes, 0, msPerTick, 0.1f, midi, ms));
  CHECK(!musicAdvance(p, notes, count, 0.0f, 0.1f, midi, ms));
  p.note = count;
  CHECK(!musicAdvance(p, notes, count, msPerTick, 0.1f, midi, ms));
  // Time running backwards counts as none
  musicPlaybackReset(p);
  CHECK(musicAdvance(p, notes, count, msPerTick, 0, midi, ms));
  musicAdvance(p, notes, count, msPerTick, -5.0f, midi, ms);
  CHECK(p.note == 0 && p.tickInNote == 0);
}

// A melody spelled out as MIDI pitch (0: rest) and sixteenths, independently of how the table writes it
struct Pinned { int midi; int ticks; };

template <size_t N>
static void checkMelody(const char* id, const Pinned (&expected)[N]) {
  const MusicSong* song = musicFindSong(id);
  CHECK(song != nullptr);
  if (!song) return;
  int total = 0;
  const int count = musicParseNotes(song->notes, notes, MUSIC_MAX_NOTES, total);
  if (count != (int)N) {
    fprintf(stderr, "  %s: %d notes, expected %d\n", id, count, (int)N);
    CHECK(false);
    return;
  }
  for (size_t i = 0; i < N; i++) {
    if (notes[i].midi != expected[i].midi || notes[i].ticks != expected[i].ticks) {
      fprintf(stderr, "  %s note %d: %d/%d, expected %d/%d\n", id, (int)i, notes[i].midi, notes[i].ticks, expected[i].midi,
              expected[i].ticks);
      CHECK(false);
    }
  }
}

static void testPinnedMelodies() {
  const int C4 = 60, D4 = 62, Eb4 = 63, E4 = 64, F4 = 65, Fs4 = 66, G4 = 67, Ab4 = 68, A4 = 69, Bb4 = 70, B4 = 71, C5 = 72,
            D5 = 74, Eb5 = 75, E5 = 76, F5 = 77;
  const Pinned entchen[] = {{C4, 2}, {D4, 2}, {E4, 2}, {F4, 2}, {G4, 4}, {G4, 4},
                            {A4, 2}, {A4, 2}, {A4, 2}, {A4, 2}, {G4, 8},
                            {A4, 2}, {A4, 2}, {A4, 2}, {A4, 2}, {G4, 8},
                            {F4, 2}, {F4, 2}, {F4, 2}, {F4, 2}, {E4, 4}, {E4, 4},
                            {D4, 2}, {D4, 2}, {D4, 2}, {D4, 2}, {C4, 8}};
  checkMelody("alle-meine-entchen", entchen);
  const Pinned twinkle[] = {{C4, 4}, {C4, 4}, {G4, 4}, {G4, 4}, {A4, 4}, {A4, 4}, {G4, 8},
                            {F4, 4}, {F4, 4}, {E4, 4}, {E4, 4}, {D4, 4}, {D4, 4}, {C4, 8},
                            {G4, 4}, {G4, 4}, {F4, 4}, {F4, 4}, {E4, 4}, {E4, 4}, {D4, 8},
                            {G4, 4}, {G4, 4}, {F4, 4}, {F4, 4}, {E4, 4}, {E4, 4}, {D4, 8},
                            {C4, 4}, {C4, 4}, {G4, 4}, {G4, 4}, {A4, 4}, {A4, 4}, {G4, 8},
                            {F4, 4}, {F4, 4}, {E4, 4}, {E4, 4}, {D4, 4}, {D4, 4}, {C4, 8}};
  checkMelody("twinkle-twinkle", twinkle);
  const Pinned frere[] = {{F4, 4}, {G4, 4}, {A4, 4}, {F4, 4}, {F4, 4}, {G4, 4}, {A4, 4}, {F4, 4},
                          {A4, 4}, {Bb4, 4}, {C5, 8}, {A4, 4}, {Bb4, 4}, {C5, 8},
                          {C5, 2}, {D5, 2}, {C5, 2}, {Bb4, 2}, {A4, 4}, {F4, 4},
                          {C5, 2}, {D5, 2}, {C5, 2}, {Bb4, 2}, {A4, 4}, {F4, 4},
                          {F4, 4}, {C4, 4}, {F4, 8}, {F4, 4}, {C4, 4}, {F4, 8}};
  checkMelody("frere-jacques", frere);
  const Pinned ode[] = {{B4, 4}, {B4, 4}, {C5, 4}, {D5, 4}, {D5, 4}, {C5, 4}, {B4, 4}, {A4, 4},
                        {G4, 4}, {G4, 4}, {A4, 4}, {B4, 4}, {B4, 6}, {A4, 2}, {A4, 8},
                        {B4, 4}, {B4, 4}, {C5, 4}, {D5, 4}, {D5, 4}, {C5, 4}, {B4, 4}, {A4, 4},
                        {G4, 4}, {G4, 4}, {A4, 4}, {B4, 4}, {A4, 6}, {G4, 2}, {G4, 8},
                        {A4, 4}, {A4, 4}, {B4, 4}, {G4, 4}, {A4, 4}, {B4, 2}, {C5, 2}, {B4, 4}, {G4, 4},
                        {A4, 4}, {B4, 2}, {C5, 2}, {B4, 4}, {A4, 4}, {G4, 4}, {A4, 4}, {D4, 4}, {B4, 4},
                        {B4, 4}, {B4, 4}, {C5, 4}, {D5, 4}, {D5, 4}, {C5, 4}, {B4, 4}, {A4, 4},
                        {G4, 4}, {G4, 4}, {A4, 4}, {B4, 4}, {A4, 6}, {G4, 2}, {G4, 8}};
  checkMelody("ode-to-joy", ode);
  const Pinned birthday[] = {{D4, 3}, {D4, 1}, {E4, 4}, {D4, 4}, {G4, 4}, {Fs4, 8},
                             {D4, 3}, {D4, 1}, {E4, 4}, {D4, 4}, {A4, 4}, {G4, 8},
                             {D4, 3}, {D4, 1}, {D5, 4}, {B4, 4}, {G4, 4}, {Fs4, 4}, {E4, 4},
                             {C5, 3}, {C5, 1}, {B4, 4}, {G4, 4}, {A4, 4}, {G4, 8}};
  checkMelody("happy-birthday", birthday);
  const Pinned stille[] = {{G4, 3}, {A4, 1}, {G4, 2}, {E4, 6}, {G4, 3}, {A4, 1}, {G4, 2}, {E4, 6},
                           {D5, 4}, {D5, 2}, {B4, 6}, {C5, 4}, {C5, 2}, {G4, 6},
                           {A4, 4}, {A4, 2}, {C5, 3}, {B4, 1}, {A4, 2}, {G4, 3}, {A4, 1}, {G4, 2}, {E4, 6},
                           {A4, 4}, {A4, 2}, {C5, 3}, {B4, 1}, {A4, 2}, {G4, 3}, {A4, 1}, {G4, 2}, {E4, 6},
                           {D5, 4}, {D5, 2}, {F5, 3}, {D5, 1}, {B4, 2}, {C5, 6}, {E5, 4}, {0, 2},
                           {C5, 3}, {G4, 1}, {E4, 2}, {G4, 3}, {F4, 1}, {D4, 2}, {C4, 6}, {C4, 4}, {0, 2}};
  checkMelody("stille-nacht", stille);
  const Pinned abend[] = {{G4, 2}, {G4, 2}, {Bb4, 6}, {G4, 2}, {G4, 4}, {Bb4, 4}, {0, 4}, {G4, 2}, {Bb4, 2},
                          {Eb5, 4}, {D5, 6}, {C5, 2}, {C5, 4}, {Bb4, 4}, {F4, 2}, {G4, 2},
                          {Ab4, 4}, {F4, 4}, {F4, 2}, {G4, 2}, {Ab4, 4}, {0, 4}, {F4, 2}, {Ab4, 2},
                          {D5, 2}, {C5, 2}, {Bb4, 4}, {D5, 4}, {Eb5, 4}, {0, 4}, {Eb4, 2}, {Eb4, 2},
                          {Eb5, 8}, {C5, 2}, {Ab4, 2}, {Bb4, 8}, {G4, 2}, {Eb4, 2}, {Ab4, 4}, {Bb4, 4}, {C5, 4},
                          {Bb4, 8}, {Eb4, 2}, {Eb4, 2}, {Eb5, 8}, {C5, 2}, {Ab4, 2}, {Bb4, 8}, {G4, 2}, {Eb4, 2},
                          {Ab4, 4}, {G4, 4}, {F4, 4}, {Eb4, 8}};
  checkMelody("guten-abend-gut-nacht", abend);
}

// What holds for every song: it fits the buffer and the staff, and its bars add up, so a dropped or
// mistyped note shows even where no melody is pinned. Bars adding up is also what makes the loop seamless.
static void testEverySong() {
  CHECK(MUSIC_SONG_COUNT >= 20);
  for (int i = 0; i < MUSIC_SONG_COUNT; i++) {
    const MusicSong& s = MUSIC_SONGS[i];
    int total = 0;
    const int count = musicParseNotes(s.notes, notes, MUSIC_MAX_NOTES, total);
    const int checksBefore = failures;
    CHECK(count > 8);
    CHECK(s.id && s.id[0] && strlen(s.id) < 40);
    CHECK(s.nameDe && s.nameDe[0] && s.nameEn && s.nameEn[0]);
    CHECK(s.beatsPerMinute >= 60 && s.beatsPerMinute <= 160);
    CHECK(s.barTicks == 8 || s.barTicks == 12 || s.barTicks == 16);
    CHECK(s.pickupTicks < s.barTicks);
    CHECK(total % s.barTicks == 0);
    for (const char* c = s.id; *c; c++) CHECK((*c >= 'a' && *c <= 'z') || *c == '-');
    int sounding = 0;
    for (int n = 0; n < count; n++) {
      const MusicNote& note = notes[n];
      if (note.midi) {
        sounding++;
        CHECK(note.staffStep >= -4 && note.staffStep <= 10);  // A3 to A5: what the band can draw
        CHECK(note.midi >= 57 && note.midi <= 81);
      }
      // No note runs over a bar line; a tie has to say so
      const int after = (note.startTick + note.ticks - s.pickupTicks) % s.barTicks;
      const int into = (note.startTick - s.pickupTicks + s.barTicks * 4) % s.barTicks;
      CHECK(after == 0 || into + note.ticks <= s.barTicks);
    }
    CHECK(sounding > 8);
    CHECK(musicFindSong(s.id) == &s);
    for (int j = 0; j < i; j++) CHECK(strcmp(MUSIC_SONGS[j].id, s.id) != 0);
    if (failures != checksBefore) fprintf(stderr, "  ... in song %s\n", s.id);
  }
}

static void testFindSong() {
  CHECK(musicFindSong("twinkle-twinkle") != nullptr);
  CHECK(musicFindSong("Twinkle-Twinkle") != nullptr);
  CHECK(musicFindSong("twinkle-twinkle")->nameEn[0] == 'T');
  const char* unknown[] = {"", " ", "twinkle", "twinkle-twinkle ", " twinkle-twinkle", "twinkle-twinkle-little-star",
                           "wheels-on-the-bus", "Alle meine Entchen", "../../etc/passwd", "alle-meine-entchen\n"};
  for (const char* s : unknown) CHECK(musicFindSong(s) == nullptr);
  CHECK(musicFindSong(nullptr) == nullptr);
}

// For check_references.py
static void dumpSongs() {
  for (int i = 0; i < MUSIC_SONG_COUNT; i++) {
    int total = 0;
    const int count = musicParseNotes(MUSIC_SONGS[i].notes, notes, MUSIC_MAX_NOTES, total);
    printf("%s", MUSIC_SONGS[i].id);
    for (int n = 0; n < count; n++) printf(" %d/%d/%d", notes[n].midi, notes[n].ticks, notes[n].tied ? 1 : 0);
    printf("\n");
  }
}

int main(int argc, char** argv) {
  if (argc > 1 && !strcmp(argv[1], "--dump")) {
    dumpSongs();
    return 0;
  }
  testParser();
  testPitch();
  testNames();
  testPlayback();
  testPinnedMelodies();
  testEverySong();
  testFindSong();
  printf("music tests: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
