// Music band: notes, pitch and playback timing. Plain C++, tested by tests/music/run.sh.
//
// Written notes look like "C4:4 F#4:2 Bb4:2 R:4 G4:4~ G4:4". Length in sixteenths (4 is a quarter),
// "~" ties to the next note, "|" is a bar line for the reader and is ignored.
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "model_core.h"
#include "name_match.h"

static const int MUSIC_MAX_NOTES = 1000;
#define MUSIC_INDEX_LIMIT (64 * 1024)  // Most the stick reads of the server's song index
#define MUSIC_FILE_LIMIT (16 * 1024)   // Most it reads of one song file
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

// What every song must satisfy, built in or from the server: it fits the buffer and the staff, and its
// bars add up, so a dropped or mistyped note is caught. Bars adding up is also what makes the loop seamless.
inline bool musicSongIsSound(const MusicNote* notes, int count, int totalTicks, int barTicks, int pickupTicks,
                             int beatsPerMinute) {
  if (!notes || count <= 8 || count > MUSIC_MAX_NOTES) return false;
  if (beatsPerMinute < 60 || beatsPerMinute > 160) return false;
  if (barTicks != 8 && barTicks != 12 && barTicks != 16) return false;
  if (pickupTicks < 0 || pickupTicks >= barTicks) return false;
  if (totalTicks % barTicks != 0) return false;
  int sounding = 0;
  for (int n = 0; n < count; n++) {
    const MusicNote& note = notes[n];
    if (note.midi) {
      sounding++;
      if (note.staffStep < -4 || note.staffStep > 10) return false;  // A3 to A5: what the band can draw
      if (note.midi < 57 || note.midi > 81) return false;
    }
    // No note runs over a bar line; a tie has to say so
    const int after = (note.startTick + note.ticks - pickupTicks) % barTicks;
    const int into = (note.startTick - pickupTicks + barTicks * 4) % barTicks;
    if (!(after == 0 || into + note.ticks <= barTicks)) return false;
  }
  return sounding > 8;
}

// The server's song index: "ZITRO-MUSIC-INDEX 1 <count>", then exactly <count> lines
// "id|de|en|aliases|source". Aliases are ';'-separated and already normalised.
struct MusicIndexEntry {
  char id[40];
  char de[48];
  char en[48];
  char source[40];
};

inline bool musicIsIdChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; }

// Copies text[0..len) into out (capacity cap) if it is non-empty, fits and has no '|' or control character.
inline bool musicCopyField(const char* text, int len, char* out, size_t cap) {
  if (len <= 0 || (size_t)len >= cap) return false;
  for (int i = 0; i < len; i++) {
    if ((unsigned char)text[i] < 0x20 || text[i] == '|') return false;
  }
  memcpy(out, text, len);
  out[len] = 0;
  return true;
}

// True if the title (already normalised) equals the normalised form of label.
inline bool musicLabelMatches(const char* label, const char* wanted) {
  char folded[96];
  return eduNormaliseName(label, folded, sizeof(folded), true) && strcmp(folded, wanted) == 0;
}

// Finds a title in the index: an exact match on the normalised form only, never a close one. The whole
// index must be sound, because a cut-off or damaged one can't be trusted for what it does contain.
// A title that matches two entries matches none.
inline bool musicResolve(const char* index, size_t indexLen, const char* title, MusicIndexEntry& out) {
  memset(&out, 0, sizeof(out));
  char wanted[96];
  if (!index || !title || indexLen == 0 || !eduNormaliseName(title, wanted, sizeof(wanted), true)) return false;
  const char* p = index;
  const char* end = index + indexLen;
  const char* line = nullptr;
  int len = sdfNextLine(p, end, line);
  static const char HEAD[] = "ZITRO-MUSIC-INDEX 1 ";
  const int headLen = (int)sizeof(HEAD) - 1;
  if (len <= headLen || memcmp(line, HEAD, headLen) != 0) return false;
  long expected = 0;
  for (int i = headLen; i < len; i++) {
    if (line[i] < '0' || line[i] > '9' || i - headLen > 4) return false;
    expected = expected * 10 + (line[i] - '0');
  }
  if (expected < 1 || expected > 2000) return false;
  long seen = 0;
  int matches = 0;
  MusicIndexEntry match;
  memset(&match, 0, sizeof(match));
  while ((len = sdfNextLine(p, end, line)) >= 0) {
    const char* lineEnd = line + len;
    const char* bar[4];
    const char* from = line;
    for (int i = 0; i < 4; i++) {
      bar[i] = (const char*)memchr(from, '|', lineEnd - from);
      if (!bar[i]) return false;
      from = bar[i] + 1;
    }
    if (memchr(from, '|', lineEnd - from)) return false;
    MusicIndexEntry current;
    memset(&current, 0, sizeof(current));
    if (!musicCopyField(line, (int)(bar[0] - line), current.id, sizeof(current.id))) return false;
    for (const char* c = current.id; *c; c++) {
      if (!musicIsIdChar(*c)) return false;
    }
    if (!musicCopyField(bar[0] + 1, (int)(bar[1] - bar[0] - 1), current.de, sizeof(current.de)) ||
        !musicCopyField(bar[1] + 1, (int)(bar[2] - bar[1] - 1), current.en, sizeof(current.en)) ||
        !musicCopyField(bar[3] + 1, (int)(lineEnd - bar[3] - 1), current.source, sizeof(current.source))) {
      return false;
    }
    bool isMatch = musicLabelMatches(current.de, wanted) || musicLabelMatches(current.en, wanted);
    const char* alias = bar[2] + 1;
    const char* aliasEnd = bar[3];
    if (alias < aliasEnd) {
      while (true) {
        const char* semi = (const char*)memchr(alias, ';', aliasEnd - alias);
        const char* stop = semi ? semi : aliasEnd;
        if (stop == alias) return false;
        if ((size_t)(stop - alias) == strlen(wanted) && memcmp(alias, wanted, stop - alias) == 0) isMatch = true;
        if (!semi) break;
        alias = semi + 1;
      }
    }
    if (isMatch) {
      matches++;
      match = current;
    }
    seen++;
    if (seen > expected) return false;
  }
  if (seen != expected || matches != 1) return false;
  out = match;
  return true;
}

// Reads a whole decimal number from text[0..len), 1 to 4 digits.
inline bool musicParseSmall(const char* text, int len, int& value) {
  if (len < 1 || len > 4) return false;
  value = 0;
  for (int i = 0; i < len; i++) {
    if (text[i] < '0' || text[i] > '9') return false;
    value = value * 10 + (text[i] - '0');
  }
  return true;
}

// A song file: exactly three lines, "ZITRO-MUSIC-SONG 1 <id>", "<bpm> <barTicks> <pickupTicks>", the notes.
// body must have room for length + 1 bytes: a terminator is written after the notes, and notes points
// into body. False for another id or any missing or extra line. The values are only checked for shape;
// musicSongIsSound judges them.
inline bool musicParseSongFile(char* body, size_t length, const char* id, int& beatsPerMinute, int& barTicks,
                               int& pickupTicks, const char*& notes) {
  notes = nullptr;
  if (!body || !id || !id[0] || length == 0 || memchr(body, 0, length)) return false;
  const char* p = body;
  const char* end = body + length;
  const char* line = nullptr;
  int len = sdfNextLine(p, end, line);
  static const char HEAD[] = "ZITRO-MUSIC-SONG 1 ";
  const int headLen = (int)sizeof(HEAD) - 1;
  if (len < 0 || len != headLen + (int)strlen(id) || memcmp(line, HEAD, headLen) != 0 ||
      memcmp(line + headLen, id, len - headLen) != 0) {
    return false;
  }
  len = sdfNextLine(p, end, line);
  if (len < 0) return false;
  int values[3];
  const char* q = line;
  const char* lineEnd = line + len;
  for (int i = 0; i < 3; i++) {
    const char* word = q;
    while (q < lineEnd && *q != ' ') q++;
    if (!musicParseSmall(word, (int)(q - word), values[i])) return false;
    if (i < 2) {
      if (q >= lineEnd) return false;
      q++;
    }
  }
  if (q != lineEnd) return false;
  const char* notesLine = nullptr;
  const int notesLen = sdfNextLine(p, end, notesLine);
  if (notesLen <= 0) return false;
  if (sdfNextLine(p, end, line) >= 0) return false;  // Nothing may follow the notes
  char* notesStart = body + (notesLine - body);
  notesStart[notesLen] = 0;
  notes = notesStart;
  beatsPerMinute = values[0];
  barTicks = values[1];
  pickupTicks = values[2];
  return true;
}
