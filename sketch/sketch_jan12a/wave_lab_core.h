// The wave lab ("Wellenlabor"): tilt the stick to play a tone and watch its wave. The math only, plain C++,
// tested on the host (tests/physics/test_wave_lab.cpp). Design: docs/superpowers/specs/2026-11-16-wave-lab-design.md
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Pitch: level is 440 Hz, 40 degrees of tilt an octave, two octaves in all
static const float WAVE_LAB_LEVEL_HZ = 440.0f;
static const float WAVE_LAB_DEG_PER_OCTAVE = 40.0f;
static const float WAVE_LAB_MIN_HZ = 220.0f;
static const float WAVE_LAB_MAX_HZ = 880.0f;
// Loudness: the start angle is the middle, 30 degrees to either end; quiet is never silent
static const float WAVE_LAB_LOUD_SPAN_DEG = 30.0f;
static const float WAVE_LAB_LOUD_MIN = 0.2f;
static const float WAVE_LAB_LOUD_MID = 0.6f;
static const float WAVE_LAB_LOUD_MAX = 1.0f;
static const float WAVE_LAB_BEAT_HZ = 4.0f;  // Two tones this far apart swell and fade 4 times a second
#define WAVE_LAB_TABLE_LEN 64                 // Samples in one cycle of a wave table
#define WAVE_LAB_HARMONICS 9                  // Overtones in a shape: 880 Hz times 9 stays under 8 kHz
static const float WAVE_LAB_PX_PER_M = 60.0f;  // The Sound scene's scale
static const uint32_t WAVE_LAB_MOVE_AWAKE_MS = 30000u;   // Tilting keeps the stick awake this long
static const uint32_t WAVE_LAB_PRESS_CAP_MS = 300000u;  // ...but never more than 5 minutes after a press

enum WaveLabShape : uint8_t { WAVE_SHAPE_SINE, WAVE_SHAPE_TRIANGLE, WAVE_SHAPE_SQUARE, WAVE_SHAPE_SAW, WAVE_SHAPE_COUNT };

// What Button B steps through. ratio 0: one tone; otherwise a second tone at hz * ratio + offsetHz (sine)
struct WaveLabPattern {
  const char* de;
  const char* en;
  uint8_t shape;
  float ratio;
  float offsetHz;
};
#define WAVE_LAB_PATTERN_COUNT 7
static const WaveLabPattern WAVE_LAB_PATTERNS[WAVE_LAB_PATTERN_COUNT] = {
    {"Sinus", "Sine", WAVE_SHAPE_SINE, 0, 0},
    {"Dreieck", "Triangle", WAVE_SHAPE_TRIANGLE, 0, 0},
    {"Rechteck", "Square", WAVE_SHAPE_SQUARE, 0, 0},
    {"S\xC3\xA4gezahn", "Sawtooth", WAVE_SHAPE_SAW, 0, 0},
    {"Oktave", "Octave", WAVE_SHAPE_SINE, 2.0f, 0},
    {"Quinte", "Fifth", WAVE_SHAPE_SINE, 1.5f, 0},
    {"Schwebung", "Beat", WAVE_SHAPE_SINE, 1.0f, WAVE_LAB_BEAT_HZ}};

inline float waveLabClamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Tilt in degrees (right edge down positive) to the tone's frequency
inline float waveLabHz(float pitchTiltDeg) {
  const float octaves = waveLabClamp(pitchTiltDeg, -WAVE_LAB_DEG_PER_OCTAVE, WAVE_LAB_DEG_PER_OCTAVE) / WAVE_LAB_DEG_PER_OCTAVE;
  return WAVE_LAB_LEVEL_HZ * exp2f(octaves);
}

// Tilt toward or away (top edge away positive) to the loudness, 0.2 to 1; zeroDeg is the middle. The difference is
// taken the short way round, so a stick held upside down doesn't jump across the 180 / -180 seam.
inline float waveLabLoudness(float loudTiltDeg, float zeroDeg) {
  const float k = waveLabClamp(remainderf(loudTiltDeg - zeroDeg, 360.0f) / WAVE_LAB_LOUD_SPAN_DEG, -1.0f, 1.0f);
  return k >= 0 ? WAVE_LAB_LOUD_MID + k * (WAVE_LAB_LOUD_MAX - WAVE_LAB_LOUD_MID)
                : WAVE_LAB_LOUD_MID + k * (WAVE_LAB_LOUD_MID - WAVE_LAB_LOUD_MIN);
}

// The accelerometer's "up" in the screen's axes (x right, y up, z towards you: gyroToScreen) to the two tilts, in
// degrees, before the caller's signs (feature_physics.ino). The left-right tilt is how far the screen's x axis
// leans from level, so it reads the same held upright or flat.
inline float waveLabPitchTiltDeg(float gx, float gy, float gz) {
  const float length = sqrtf(gx * gx + gy * gy + gz * gz);
  if (length < 1e-6f) return 0;
  return asinf(waveLabClamp(gx / length, -1.0f, 1.0f)) * 180.0f / (float)M_PI;
}
// The lean toward or away: 0 upright, 90 lying flat. Measured against the whole of up in the screen's plane, so
// turning the stick for the pitch doesn't change it (exact up to a quarter turn)
inline float waveLabLoudTiltDeg(float gx, float gy, float gz) {
  const float inPlane = copysignf(hypotf(gx, gy), gy);
  if (fabsf(inPlane) < 1e-6f && fabsf(gz) < 1e-6f) return 0;
  return atan2f(gz, inPlane) * 180.0f / (float)M_PI;
}

// One cycle of a shape before scaling, built from its first WAVE_LAB_HARMONICS harmonics
inline float waveLabShapeRaw(uint8_t shape, float turns) {
  const float a = 2.0f * (float)M_PI * turns;
  if (shape == WAVE_SHAPE_SINE || shape >= WAVE_SHAPE_COUNT) return sinf(a);
  float sum = 0;
  for (int n = 1; n <= WAVE_LAB_HARMONICS; n++) {
    const bool odd = n % 2 == 1;
    if (shape == WAVE_SHAPE_SQUARE && odd) sum += sinf(n * a) / n;
    if (shape == WAVE_SHAPE_SAW) sum += sinf(n * a) / n;
    if (shape == WAVE_SHAPE_TRIANGLE && odd) sum += ((n / 2) % 2 ? -1.0f : 1.0f) * sinf(n * a) / (float)(n * n);
  }
  return sum;
}

// The shape's value at a point of its cycle (turns, any real number), scaled so that its peak is 1
inline float waveLabShapeValue(uint8_t shape, float turns) {
  static float scale[WAVE_SHAPE_COUNT] = {0, 0, 0, 0};
  if (shape >= WAVE_SHAPE_COUNT) shape = WAVE_SHAPE_SINE;
  if (scale[shape] == 0) {
    float top = 0;
    for (int i = 0; i < 2048; i++) top = fmaxf(top, fabsf(waveLabShapeRaw(shape, i / 2048.0f)));
    scale[shape] = top > 0 ? 1.0f / top : 1.0f;
  }
  turns -= floorf(turns);
  return waveLabShapeRaw(shape, turns) * scale[shape];
}

// One cycle as the speaker plays it: 8-bit unsigned around 128. Filled once and never changed, because the
// speaker reads it while the tone plays.
inline const uint8_t* waveLabTable(uint8_t shape) {
  static uint8_t tables[WAVE_SHAPE_COUNT][WAVE_LAB_TABLE_LEN];
  static bool filled = false;
  if (!filled) {
    for (int s = 0; s < WAVE_SHAPE_COUNT; s++) {
      for (int i = 0; i < WAVE_LAB_TABLE_LEN; i++) {
        tables[s][i] = (uint8_t)(128 + lroundf(115.0f * waveLabShapeValue((uint8_t)s, (float)i / WAVE_LAB_TABLE_LEN)));
      }
    }
    filled = true;
  }
  return tables[shape < WAVE_SHAPE_COUNT ? shape : WAVE_SHAPE_SINE];
}

// The pattern's second tone for a first tone at hz, 0 if it has none
inline float waveLabSecondHz(const WaveLabPattern& p, float hz) {
  return p.ratio > 0 ? hz * p.ratio + p.offsetHz : 0;
}

// The nearest note, with its octave number ("A4"). Names as in the music view: H and the sharps in German.
inline void waveLabNoteName(float hz, bool german, char* out, size_t size) {
  if (!out || size == 0) return;
  static const char* const DE[12] = {"C", "Cis", "D", "Dis", "E", "F", "Fis", "G", "Gis", "A", "Ais", "H"};
  static const char* const EN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  if (!(hz > 0)) {
    out[0] = 0;
    return;
  }
  const long midi = lroundf(69.0f + 12.0f * log2f(hz / 440.0f));
  const int name = (int)(((midi % 12) + 12) % 12);
  const long octave = (midi - name) / 12 - 1;
  snprintf(out, size, "%s%ld", (german ? DE : EN)[name], octave);
}

// True while the sound may hold off the sleep timers: tilted lately, and a press within the last 5 minutes
inline bool waveLabPressTimedOut(uint32_t now, uint32_t lastPress) { return now - lastPress >= WAVE_LAB_PRESS_CAP_MS; }
inline bool waveLabKeepAwake(uint32_t now, uint32_t lastMove, uint32_t lastPress) {
  return now - lastMove < WAVE_LAB_MOVE_AWAKE_MS && !waveLabPressTimedOut(now, lastPress);
}

// The speaker channel's volume for a loudness 0..1 and the tone's share of it (1, or 0.5 for each of two tones).
// M5Unified's mixer squares a channel's volume, so this is the square root: the sound then follows the loudness
// the screen shows, quiet a fifth of full, and two tones at half each add up to one.
inline uint8_t waveLabChannelVolume(float loudness, float share) {
  const float v = loudness * share;
  return (uint8_t)lroundf(255.0f * sqrtf(v > 0 ? (v < 1 ? v : 1) : 0));
}

// The held sound (feature_physics.ino). Each frame sends one long piece that takes over from the playing one where
// its wave stands (M5Unified keeps the place in the table when the table stays the same), so the sound never runs
// out between frames, which come 40 to 60 ms apart; with short pieces queued behind each other it did, and clicked.
// Frames that stop let it end by itself.
static const uint32_t WAVE_LAB_PIECE_MS = 200;   // A dark screen stops the frames: quiet within a quarter second
static const uint32_t WAVE_LAB_FINISH_MS = 30;   // A new shape: the old one's last whole cycles, then the new one

// How many whole cycles M5.Speaker.tone(hz, ms, ...) plays: its own formula. 0 would play without end.
inline uint32_t waveLabToneCycles(uint32_t ms, float hz) { return (uint32_t)(ms * hz / 1000); }

// What a speaker channel does in a frame, from whether it sounds and with which shape, and what the scene wants
enum WaveLabSoundStep : uint8_t {
  WAVE_LAB_STEP_NONE,    // Silent and stays so
  WAVE_LAB_STEP_STOP,    // Goes quiet
  WAVE_LAB_STEP_START,   // Starts at the beginning of a cycle
  WAVE_LAB_STEP_RETUNE,  // The same shape at the new pitch, from where its wave stands
  WAVE_LAB_STEP_SWITCH   // Another shape: the old one to the end of a cycle, the new one from the start of its own
};

inline WaveLabSoundStep waveLabSoundStep(bool sounding, uint8_t playingShape, bool wanted, uint8_t wantedShape) {
  if (!wanted) return sounding ? WAVE_LAB_STEP_STOP : WAVE_LAB_STEP_NONE;
  if (!sounding) return WAVE_LAB_STEP_START;
  return playingShape == wantedShape ? WAVE_LAB_STEP_RETUNE : WAVE_LAB_STEP_SWITCH;
}

// Angles in degrees that cross from 180 to -180: the difference the short way round, and a step toward target
inline float waveLabAngleDiff(float to, float from) { return remainderf(to - from, 360.0f); }
inline float waveLabAngleStep(float current, float target, float k) {
  float r = current + waveLabAngleDiff(target, current) * k;
  r = remainderf(r, 360.0f);
  return r <= -180.0f ? r + 360.0f : r;
}

// Turned further than this for the pitch (toward portrait), the lean can't be read: the loudness holds
static const float WAVE_LAB_LOUD_MAX_ROLL_DEG = 70.0f;
inline bool waveLabLoudReadable(float pitchTiltDeg) { return fabsf(pitchTiltDeg) <= WAVE_LAB_LOUD_MAX_ROLL_DEG; }

// The second tone, as the screen shows it: "+ 880 Hz" for the octave and the fifth, "+ 4 Hz" for the beat (how far
// apart the two are is what it's about). Empty for one tone.
inline void waveLabSecondText(const WaveLabPattern& p, float hz, char* out, size_t size) {
  if (!out || size == 0) return;
  out[0] = 0;
  if (p.ratio <= 0) return;
  if (p.offsetHz > 0) {
    snprintf(out, size, "+ %ld Hz", lroundf(p.offsetHz));
  } else {
    snprintf(out, size, "+ %ld Hz", lroundf(waveLabSecondHz(p, hz)));
  }
}

// One wavelength in air at 20 °C, as the screen shows it: "λ 78 cm" below a metre, else "λ 1,56 m" (German decimal
// mark). Rounded first, then the unit chosen, so it never reads "100 cm".
inline void waveLabWavelengthText(float hz, bool german, char* out, size_t size) {
  if (!out || size == 0) return;
  out[0] = 0;
  if (!(hz > 0)) return;
  const float metres = 343.0f / hz;  // PHYSICS_SOUND_SPEED
  const long cm = lroundf(metres * 100.0f);
  if (cm < 100) {
    snprintf(out, size, "\xCE\xBB %ld cm", cm);
    return;
  }
  snprintf(out, size, "\xCE\xBB %.2f m", (double)metres);
  if (german) {
    for (char* p = out; *p; p++) {
      if (*p == '.') *p = ',';
    }
  }
}

// Sound waves on the speaker symbol, 0 to 3, as music's speaker: what you hear, from Zitro's volume (0..255, as
// M5.Speaker.getVolume() reports it) times the tilt's loudness. None with the sound off or the volume at 0; one as
// soon as anything sounds.
inline int waveLabSpeakerWaves(bool soundOn, int deviceVolume, float loudness) {
  if (!soundOn || deviceVolume <= 0 || !(loudness > 0)) return 0;
  const float heard = waveLabClamp(deviceVolume / 255.0f, 0, 1) * waveLabClamp(loudness, 0, 1);
  const int waves = (int)ceilf(3.0f * heard - 1e-4f);
  return waves < 1 ? 1 : (waves > 3 ? 3 : waves);
}
