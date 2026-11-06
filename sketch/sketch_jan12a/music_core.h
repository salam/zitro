// Music band: notes, pitch and playback timing. Plain C++, tested by tests/music/run.sh.
//
// Written notes look like "C4:4 F#4:2 Bb4:2 R:4 G4:4~ G4:4". Length in sixteenths (4 is a quarter),
// "~" ties to the next note, "|" is a bar line for the reader and is ignored.
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

static const int MUSIC_MAX_NOTES = 200;
static const int MUSIC_TICKS_PER_QUARTER = 4;
static const float MUSIC_PLAY_CAP_SECONDS = 5 * 60;  // Then it pauses, so a forgotten stick can still sleep
static const float MUSIC_GAP_SHARE = 0.12f;          // Silence before the next note, so repeated notes stay apart

struct MusicNote {
  uint16_t startTick;
  uint8_t ticks;
  uint8_t midi;       // 0: rest
  int8_t staffStep;   // Steps above the bottom staff line (E4); a line every second step
  int8_t accidental;  // -1 flat, 0 none, 1 sharp
  uint8_t letter;     // 0..6 for C..B
  bool tied;          // Sounds on into the next note, which has the same pitch
};

inline bool musicIsAllowedLength(int ticks) {
  return ticks == 1 || ticks == 2 || ticks == 3 || ticks == 4 || ticks == 6 || ticks == 8 || ticks == 12 || ticks == 16;
}

// Reads one length after ':'. Moves p past it. 0 if there is none.
inline int musicParseLength(const char*& p) {
  if (*p != ':') return 0;
  p++;
  int value = 0, digits = 0;
  while (*p >= '0' && *p <= '9' && digits < 3) {
    value = value * 10 + (*p - '0');
    p++;
    digits++;
  }
  return digits > 0 ? value : 0;
}

// Number of notes, or -1 if anything is wrong: then none of it may be used.
inline int musicParseNotes(const char* text, MusicNote* out, int capacity, int& totalTicks) {
  totalTicks = 0;
  if (!text || !out || capacity <= 0) return -1;
  static const uint8_t SEMITONES[7] = {0, 2, 4, 5, 7, 9, 11};
  int count = 0;
  long tick = 0;
  const char* p = text;
  while (*p) {
    if (*p == ' ' || *p == '|') {
      p++;
      continue;
    }
    if (count >= capacity) return -1;
    MusicNote n = {};
    if (*p == 'R') {
      p++;
      n.ticks = 0;
    } else {
      const char* letters = "CDEFGAB";
      const char* found = *p ? strchr(letters, *p) : nullptr;
      if (!found) return -1;
      n.letter = (uint8_t)(found - letters);
      p++;
      if (*p == '#') {
        n.accidental = 1;
        p++;
      } else if (*p == 'b') {
        n.accidental = -1;
        p++;
      }
      if (*p < '2' || *p > '6') return -1;
      const int octave = *p - '0';
      p++;
      n.midi = (uint8_t)(12 * (octave + 1) + SEMITONES[n.letter] + n.accidental);
      n.staffStep = (int8_t)(octave * 7 + n.letter - (4 * 7 + 2));
    }
    const int length = musicParseLength(p);
    if (!musicIsAllowedLength(length)) return -1;
    n.ticks = (uint8_t)length;
    if (*p == '~') {
      if (n.midi == 0) return -1;
      n.tied = true;
      p++;
    }
    if (*p != 0 && *p != ' ' && *p != '|') return -1;
    if (count > 0 && out[count - 1].tied && out[count - 1].midi != n.midi) return -1;
    if (tick + length > 60000) return -1;
    n.startTick = (uint16_t)tick;
    tick += length;
    out[count++] = n;
  }
  if (count == 0 || out[count - 1].tied) return -1;
  totalTicks = (int)tick;
  return count;
}

inline float musicFrequencyHz(uint8_t midi) {
  return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

// The name a child learns in that language: in German B natural is H and B flat is B.
inline const char* musicNoteName(const MusicNote& n, bool german) {
  if (n.midi == 0 || n.letter > 6 || n.accidental < -1 || n.accidental > 1) return "";
  static const char* const DE[3][7] = {{"Ces", "Des", "Es", "Fes", "Ges", "As", "B"},
                                       {"C", "D", "E", "F", "G", "A", "H"},
                                       {"Cis", "Dis", "Eis", "Fis", "Gis", "Ais", "His"}};
  static const char* const EN[3][7] = {{"Cb", "Db", "Eb", "Fb", "Gb", "Ab", "Bb"},
                                       {"C", "D", "E", "F", "G", "A", "B"},
                                       {"C#", "D#", "E#", "F#", "G#", "A#", "B#"}};
  return (german ? DE : EN)[n.accidental + 1][n.letter];
}

struct MusicPlayback {
  int note;          // The note at the playhead
  float tickInNote;  // How far into it
  bool started;      // False: the next step sounds the current note without moving on
  bool paused;
  float playedSeconds;  // Since opening or the last resume, for the cap
};

inline void musicPlaybackReset(MusicPlayback& p) {
  p.note = 0;
  p.tickInNote = 0;
  p.started = false;
  p.paused = false;
  p.playedSeconds = 0;
}

// What Button C steps through while a song plays. Loud is the stored volume setting.
// The value is also the number of sound waves on the speaker symbol.
enum MusicVolumeLevel { MUSIC_VOLUME_SILENT, MUSIC_VOLUME_LOW, MUSIC_VOLUME_REGULAR, MUSIC_VOLUME_LOUD };
static const int MUSIC_VOLUME_START = MUSIC_VOLUME_LOUD;

inline int musicNextVolumeLevel(int level) {
  return (level + 1) % (MUSIC_VOLUME_LOUD + 1);
}

inline int musicVolumeAt(int stored, int level) {
  if (stored < 0) stored = 0;
  if (stored > 255) stored = 255;
  return stored * level / MUSIC_VOLUME_LOUD;
}

// Silent starts no tones at all: the buzzer clicks when a tone starts, even at volume 0.
inline bool musicVolumeSounds(int level) {
  return level != MUSIC_VOLUME_SILENT;
}

inline float musicMsPerTick(int beatsPerMinute) {
  return 60000.0f / (beatsPerMinute * MUSIC_TICKS_PER_QUARTER);
}

// Pause or carry on. Carrying on sounds the rest of the current note.
inline void musicTogglePause(MusicPlayback& p) {
  p.paused = !p.paused;
  if (!p.paused) {
    p.started = false;
    p.playedSeconds = 0;
  }
}

// Moves on by seconds. True if a tone must start now, with toneMidi and toneMs. The tone ends by itself,
// so sound can't hang if frames stop coming. After a long stall it skips ahead to the note it should be on.
inline bool musicAdvance(MusicPlayback& p, const MusicNote* notes, int count, float msPerTick, float seconds,
                         uint8_t& toneMidi, uint32_t& toneMs) {
  toneMidi = 0;
  toneMs = 0;
  if (p.paused || !notes || count <= 0 || msPerTick <= 0 || p.note < 0 || p.note >= count) return false;
  if (seconds < 0) seconds = 0;
  p.playedSeconds += seconds;
  if (p.playedSeconds >= MUSIC_PLAY_CAP_SECONDS) {
    p.paused = true;
    return false;
  }
  bool sound = false;
  if (!p.started) {
    p.started = true;
    sound = true;
  } else {
    p.tickInNote += seconds * 1000.0f / msPerTick;
    while (p.tickInNote >= notes[p.note].ticks) {
      p.tickInNote -= notes[p.note].ticks;
      if (!notes[p.note].tied) sound = true;  // A tied note goes on sounding
      p.note = (p.note + 1) % count;
    }
  }
  if (!sound || notes[p.note].midi == 0) return false;
  float ticksLeft = notes[p.note].ticks - p.tickInNote;
  for (int i = p.note; notes[i].tied && i + 1 < count; i++) ticksLeft += notes[i + 1].ticks;
  toneMidi = notes[p.note].midi;
  float ms = ticksLeft * msPerTick * (1.0f - MUSIC_GAP_SHARE);
  toneMs = ms < 1.0f ? 1 : (uint32_t)ms;
  return true;
}
