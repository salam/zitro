// Drawing for the wave lab: the wave at its true length in air, its pitch, note and pattern, and the sound the
// scene wants. Included from physics_draw.h, which holds PhysicsState and the shared helpers. The math is in
// wave_lab_core.h; the device reads the tilt and plays labSound (feature_physics.ino).
#pragma once

static const float PHYSICS_LAB_SMOOTH_S = 0.15f;  // The tilt settles this fast: a steady hand gives a steady tone
static const int PHYSICS_LAB_WAVE_Y = 36;         // Middle of the wave band
static const float PHYSICS_LAB_WAVE_HEIGHT = 26;  // Pixels at full loudness

// Switches the held sound; switched on, the loudness tilt of the moment becomes the middle again
inline void physicsWaveLabSoundSwitch(PhysicsState& s, bool on) {
  s.labSoundOn = on;
  if (on) s.data.lab.zeroPending = true;
}

// "≈": the fonts have no glyph for it, so two small waves
template <class C>
void physicsDrawAbout(C& c, int x, int baseline, uint16_t color) {
  for (int row = 0; row < 2; row++) {
    const int y = baseline - 5 - row * 4;
    c.drawLine(x, y + 1, x + 2, y, color);
    c.drawLine(x + 2, y, x + 4, y + 1, color);
    c.drawLine(x + 4, y + 1, x + 6, y, color);
  }
}

// A small speaker with 0 to 3 sound waves for what you hear (waveLabSpeakerWaves); white while the sound is on
template <class C>
void physicsDrawLabSpeaker(C& c, int x, int cy, bool on, int waves) {
  const uint16_t color = on ? PHYSICS_WHITE : PHYSICS_GREY;
  c.fillRect(x, cy - 3, 4, 6, color);
  c.fillTriangle(x + 3, cy - 3, x + 8, cy - 7, x + 8, cy + 3, color);
  c.fillTriangle(x + 3, cy + 3, x + 8, cy - 3, x + 8, cy + 7, color);
  for (int i = 0; i < waves && i < 3; i++) {
    const int reach = 2 + 2 * i;
    c.drawFastVLine(x + 10 + 4 * i, cy - reach, 2 * reach + 1, color);
    c.drawFastVLine(x + 11 + 4 * i, cy - reach, 2 * reach + 1, color);
  }
}

// A wave across the screen: value(x) is -1..1, drawn around the band's middle
template <class C, class F>
void physicsDrawLabCurve(C& c, F value, float height, uint16_t color, bool thick) {
  int lastY = PHYSICS_LAB_WAVE_Y - (int)lroundf(height * value(0));
  for (int x = 2; x < PHYSICS_W; x += 2) {
    const int y = PHYSICS_LAB_WAVE_Y - (int)lroundf(height * value(x));
    if (thick) {
      physicsThickLine(c, x - 2, lastY, x, y, color);
    } else {
      c.drawLine(x - 2, lastY, x, y, color);
    }
    lastY = y;
  }
}

template <class C>
void physicsDrawWaveLab(C& c, PhysicsState& s, float dt) {
  PhysicsWaveLabData& d = s.data.lab;
  // The tilt, smoothed; the first frame takes it as it is
  const float k = dt <= 0 ? 1.0f : (dt / PHYSICS_LAB_SMOOTH_S > 1.0f ? 1.0f : dt / PHYSICS_LAB_SMOOTH_S);
  d.pitchDeg += (s.tiltPitchDeg - d.pitchDeg) * k;
  // The lean the short way round (upside down it crosses 180 / -180), and not at all when turned toward portrait,
  // where it can't be read
  if (waveLabLoudReadable(s.tiltPitchDeg)) d.loudDeg = waveLabAngleStep(d.loudDeg, s.tiltLoudDeg, k);
  if (d.zeroPending) {
    d.loudZeroDeg = d.loudDeg;
    d.zeroPending = false;
  }
  const WaveLabPattern& pattern = WAVE_LAB_PATTERNS[s.param < WAVE_LAB_PATTERN_COUNT ? s.param : 0];
  const float hz = waveLabHz(d.pitchDeg);
  const float second = waveLabSecondHz(pattern, hz);
  const float loud = waveLabLoudness(d.loudDeg, d.loudZeroDeg);
  const float lambdaPx = PHYSICS_SOUND_SPEED / hz * WAVE_LAB_PX_PER_M;

  // The wave at the left edge moves on as the wave travels right (slowed); a changing pitch then stretches the wave
  // from there instead of making it jump. Two cycles of the lower tone hold whole cycles of the octave and the fifth.
  s.t += dt;
  d.realT += dt;
  if (d.realT > 64.0f) d.realT -= 64.0f;
  d.phase -= dt * PHYSICS_SOUND_SCREEN_SPEED / lambdaPx;
  d.phase -= 2.0f * floorf(d.phase / 2.0f);
  const float phase = d.phase;
  const float height = PHYSICS_LAB_WAVE_HEIGHT * loud;

  // One wavelength, marked under the wave from where a cycle starts, so it travels along with the wave
  const int bracketY = PHYSICS_LAB_WAVE_Y + (int)PHYSICS_LAB_WAVE_HEIGHT + 6;
  const float startTurn = ceilf(phase) - phase;  // 0..1 of a cycle to the first start
  const int bracketStart = (int)lroundf(startTurn * lambdaPx);
  const int bracketEnd = bracketStart + (int)lroundf(lambdaPx);
  c.drawFastHLine(bracketStart, bracketY, bracketEnd - bracketStart, PHYSICS_LIGHT);
  c.drawFastVLine(bracketStart, bracketY - 3, 7, PHYSICS_LIGHT);
  c.drawFastVLine(bracketEnd, bracketY - 3, 7, PHYSICS_LIGHT);

  if (pattern.ratio == 0) {
    physicsDrawLabCurve(c, [&](int x) { return waveLabShapeValue(pattern.shape, phase + x / lambdaPx); }, height,
                        PHYSICS_CITRUS, true);
  } else if (pattern.offsetHz > 0) {
    // A beat: in a few metres of air the two tones are one wave whose height swells and fades with time
    const float swell = fabsf(cosf(PHYSICS_PI * pattern.offsetHz * d.realT));
    physicsDrawLabCurve(c, [&](int x) { return swell * sinf(2.0f * PHYSICS_PI * (phase + x / lambdaPx)); }, height,
                        PHYSICS_CITRUS, true);
  } else {
    const float ratio = pattern.ratio;
    auto low = [&](int x) { return 0.5f * sinf(2.0f * PHYSICS_PI * (phase + x / lambdaPx)); };
    auto high = [&](int x) { return 0.5f * sinf(2.0f * PHYSICS_PI * ratio * (phase + x / lambdaPx)); };
    physicsDrawLabCurve(c, low, height, PHYSICS_GREY, false);
    physicsDrawLabCurve(c, high, height, PHYSICS_GREY, false);
    physicsDrawLabCurve(c, [&](int x) { return low(x) + high(x); }, height, PHYSICS_CITRUS, true);
  }

  // Frequency, wavelength, and the note or the second tone
  char text[24];
  const int row1 = 90, row2 = 112;
  physicsValue(text, sizeof(text), hz, 0, "Hz", s.german);
  physicsText(c, s.normal, text, 2, row1, PHYSICS_CITRUS, textdatum_t::baseline_left);
  waveLabWavelengthText(hz, s.german, text, sizeof(text));
  physicsText(c, s.normal, text, 76, row1, PHYSICS_WHITE, textdatum_t::baseline_left);
  if (second > 0) {
    waveLabSecondText(pattern, hz, text, sizeof(text));  // "+ 880 Hz"; the beat "+ 4 Hz"
    physicsText(c, s.normal, text, PHYSICS_W - 2, row1, PHYSICS_WHITE, textdatum_t::baseline_right);
  } else {
    waveLabNoteName(hz, s.german, text, sizeof(text));
    c.setFreeFont(s.normal);
    const int noteX = PHYSICS_W - 2 - c.textWidth(text);
    physicsText(c, s.normal, text, PHYSICS_W - 2, row1, PHYSICS_WHITE, textdatum_t::baseline_right);
    physicsDrawAbout(c, noteX - 10, row1, PHYSICS_WHITE);
  }

  // The pattern, "slowed", and whether the sound is on
  physicsText(c, s.normal, s.german ? pattern.de : pattern.en, 2, row2, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsText(c, s.normal, physicsPick(s, "Zeitlupe", "slowed"), 158, row2, PHYSICS_GREY, textdatum_t::baseline_center);
  physicsDrawLabSpeaker(c, PHYSICS_W - 20, row2 - 6, s.labSoundOn, waveLabSpeakerWaves(s.labSoundOn, s.deviceVolume, loud));

  // The sound to play: what is drawn, or nothing
  s.labSound.count = s.labSoundOn ? (second > 0 ? 2 : 1) : 0;
  s.labSound.hz[0] = hz;
  s.labSound.hz[1] = second;
  s.labSound.shape = pattern.ratio > 0 ? (uint8_t)WAVE_SHAPE_SINE : pattern.shape;
  s.labSound.loudness = loud;
}
