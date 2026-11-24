// The wave lab's math: tilt to pitch and loudness, note names, the wave shapes and their tables, staying awake.
#include <cstring>
#include <string>
#include "test_common.h"
#include "../../sketch/sketch_jan12a/wave_lab_core.h"

static void testPitch() {
  NEAR(waveLabHz(0), 440, 0.01);
  NEAR(waveLabHz(40), 880, 0.01);
  NEAR(waveLabHz(-40), 220, 0.01);
  NEAR(waveLabHz(90), 880, 0.01);
  NEAR(waveLabHz(-90), 220, 0.01);
  NEAR(waveLabHz(20), 622.25, 0.05);  // Half an octave up
  for (int d = -40; d < 40; d++) CHECK(waveLabHz((float)d + 1) > waveLabHz((float)d));
}

static void testLoudness() {
  NEAR(waveLabLoudness(10, 10), 0.6, 1e-5);
  NEAR(waveLabLoudness(40, 10), 1.0, 1e-5);
  NEAR(waveLabLoudness(-20, 10), 0.2, 1e-5);
  NEAR(waveLabLoudness(100, 10), 1.0, 1e-5);
  NEAR(waveLabLoudness(-100, 10), 0.2, 1e-5);
  for (int d = -180; d <= 180; d++) CHECK(waveLabLoudness((float)d, 0) >= 0.2f - 1e-6f);
}

static void testTiltAngles() {
  NEAR(waveLabPitchTiltDeg(0, 1, 0), 0, 1e-4);
  NEAR(waveLabLoudTiltDeg(0, 1, 0), 0, 1e-4);
  NEAR(waveLabPitchTiltDeg(0.5f, 0.866f, 0), 30, 0.1);
  NEAR(waveLabPitchTiltDeg(0, 0, 1), 0, 1e-4);
  NEAR(waveLabLoudTiltDeg(0, 0, 1), 90, 0.1);
  NEAR(waveLabPitchTiltDeg(0.5f, 0, 0.866f), 30, 0.1);
  NEAR(waveLabPitchTiltDeg(0, -1, 0), 0, 1e-4);
  NEAR(fabs(waveLabLoudTiltDeg(0, -1, 0)), 180, 0.1);
  CHECK(std::isfinite(waveLabPitchTiltDeg(0, 0, 0)));
  CHECK(std::isfinite(waveLabLoudTiltDeg(0, 0, 0)));
  CHECK(std::isfinite(waveLabLoudTiltDeg(2, 0, 0)));
  NEAR(waveLabPitchTiltDeg(2, 0, 0), 90, 0.1);
}

// Size of harmonic n in one cycle of the shape
static double harmonic(uint8_t shape, int n) {
  const int points = 4096;
  double re = 0, im = 0;
  for (int i = 0; i < points; i++) {
    const double turns = (double)i / points;
    const double v = waveLabShapeValue(shape, (float)turns);
    re += v * cos(2 * M_PI * n * turns);
    im += v * sin(2 * M_PI * n * turns);
  }
  return 2 * sqrt(re * re + im * im) / points;
}

static void testShapes() {
  for (uint8_t s = 0; s < WAVE_SHAPE_COUNT; s++) {
    CHECK(harmonic(s, 1) > 0.1);
    for (int n = WAVE_LAB_HARMONICS + 1; n <= 20; n++) CHECK(harmonic(s, n) < 1e-3);
    double top = 0, sum = 0;
    for (int i = 0; i < 4096; i++) {
      const double v = waveLabShapeValue(s, i / 4096.0f);
      top = fmax(top, fabs(v));
      sum += v;
    }
    NEAR(top, 1.0, 0.01);
    NEAR(sum / 4096, 0.0, 1e-3);
  }
  for (int n = 2; n <= WAVE_LAB_HARMONICS; n++) CHECK(harmonic(WAVE_SHAPE_SINE, n) < 1e-3);
  for (int n = 2; n <= WAVE_LAB_HARMONICS; n += 2) {
    CHECK(harmonic(WAVE_SHAPE_SQUARE, n) < 1e-3);
    CHECK(harmonic(WAVE_SHAPE_TRIANGLE, n) < 1e-3);
  }
  for (int n = 3; n <= WAVE_LAB_HARMONICS; n += 2) {
    CHECK(harmonic(WAVE_SHAPE_SQUARE, n) > 1e-3);
    CHECK(harmonic(WAVE_SHAPE_TRIANGLE, n) > 1e-3);
  }
  for (int n = 2; n <= WAVE_LAB_HARMONICS; n++) CHECK(harmonic(WAVE_SHAPE_SAW, n) > 1e-3);
  // The square's overtones fall as 1/n, the triangle's as 1/n^2
  NEAR(harmonic(WAVE_SHAPE_SQUARE, 3) / harmonic(WAVE_SHAPE_SQUARE, 1), 1.0 / 3, 1e-3);
  NEAR(harmonic(WAVE_SHAPE_TRIANGLE, 3) / harmonic(WAVE_SHAPE_TRIANGLE, 1), 1.0 / 9, 1e-3);
  NEAR(harmonic(WAVE_SHAPE_SAW, 2) / harmonic(WAVE_SHAPE_SAW, 1), 1.0 / 2, 1e-3);
}

static void testTables() {
  for (uint8_t s = 0; s < WAVE_SHAPE_COUNT; s++) {
    const uint8_t* t = waveLabTable(s);
    CHECK(t != nullptr);
    if (!t) continue;
    CHECK(waveLabTable(s) == t);  // The speaker keeps the pointer: it must stay put
    long sum = 0;
    for (int i = 0; i < WAVE_LAB_TABLE_LEN; i++) {
      CHECK(t[i] >= 13 && t[i] <= 243);
      sum += t[i];
    }
    NEAR((double)sum / WAVE_LAB_TABLE_LEN, 128, 1.0);
    CHECK(t[0] == 128 + lround(115 * waveLabShapeValue(s, 0)));
  }
  CHECK(waveLabTable(WAVE_SHAPE_COUNT) != nullptr);  // Out of range: a sine, never a stray pointer
}

static void testPatterns() {
  CHECK(WAVE_LAB_PATTERN_COUNT == 7);
  const char* de[] = {"Sinus", "Dreieck", "Rechteck", "S\xC3\xA4gezahn", "Oktave", "Quinte", "Schwebung"};
  const char* en[] = {"Sine", "Triangle", "Square", "Sawtooth", "Octave", "Fifth", "Beat"};
  const float second[] = {0, 0, 0, 0, 880, 660, 444};
  for (int i = 0; i < WAVE_LAB_PATTERN_COUNT; i++) {
    CHECK(strcmp(WAVE_LAB_PATTERNS[i].de, de[i]) == 0);
    CHECK(strcmp(WAVE_LAB_PATTERNS[i].en, en[i]) == 0);
    NEAR(waveLabSecondHz(WAVE_LAB_PATTERNS[i], 440), second[i], 1e-3);
    CHECK(WAVE_LAB_PATTERNS[i].shape < WAVE_SHAPE_COUNT);
  }
}

static std::string note(float hz, bool german) {
  char out[16];
  waveLabNoteName(hz, german, out, sizeof(out));
  return out;
}

static void testNotes() {
  CHECK(note(440, true) == "A4");
  CHECK(note(440, false) == "A4");
  CHECK(note(261.63f, true) == "C4");
  CHECK(note(493.88f, true) == "H4");
  CHECK(note(493.88f, false) == "B4");
  CHECK(note(466.16f, true) == "Ais4");
  CHECK(note(466.16f, false) == "A#4");
  CHECK(note(450, true) == "A4");
  CHECK(note(455, true) == "Ais4");
  CHECK(note(455, false) == "A#4");
  CHECK(note(220, true) == "A3");
  CHECK(note(880, true) == "A5");
  char tiny[4];
  waveLabNoteName(466.16f, true, tiny, sizeof(tiny));  // "Ais4" doesn't fit: cut, never overflowed
  CHECK(strlen(tiny) == 3);
}

static void testAwake() {
  CHECK(waveLabKeepAwake(100000, 80000, 90000));
  CHECK(!waveLabKeepAwake(100000, 60000, 90000));
  CHECK(!waveLabKeepAwake(400000, 399000, 99999));  // Five minutes after the last press
  CHECK(waveLabPressTimedOut(300000, 0));
  CHECK(!waveLabPressTimedOut(299999, 0));
  CHECK(waveLabKeepAwake(5, 0xFFFFFFF0u, 0xFFFFFFF0u));  // millis() wrapped
}

// The mixer squares a channel's volume (M5Unified 0.2.12 and 0.2.29), so the volume is the square root of the
// loudness wanted: quiet stays a fifth of full, and two tones at half each add up to one
static void testChannelVolume() {
  CHECK(waveLabChannelVolume(1.0f, 1.0f) == 255);
  CHECK(waveLabChannelVolume(0.2f, 1.0f) == 114);
  CHECK(waveLabChannelVolume(0.6f, 1.0f) == 198);
  CHECK(waveLabChannelVolume(1.0f, 0.5f) == 180);
  for (float loud = 0.2f; loud <= 1.0f; loud += 0.05f) {
    const float v = waveLabChannelVolume(loud, 1.0f) / 255.0f;
    NEAR(v * v, loud, 0.01);
  }
}

// Leaning back 20 degrees reads 20 degrees however far the stick is turned for the pitch
static void testLoudIgnoresPitchTilt() {
  const float lean = 20.0f * (float)M_PI / 180.0f;
  for (int deg = 0; deg <= 80; deg += 5) {
    const float roll = deg * (float)M_PI / 180.0f;
    const float ux = -sinf(roll) * cosf(lean), uy = cosf(roll) * cosf(lean), uz = sinf(lean);
    NEAR(waveLabLoudTiltDeg(ux, uy, uz), 20, 0.05);
  }
  NEAR(waveLabLoudTiltDeg(0, 0, 1), 90, 0.1);  // Flat on the table, as before
}

// Upside down the reading crosses from 180 to -180: that is 2 degrees, not 358
static void testLoudAcrossTheSeam() {
  NEAR(waveLabLoudness(179, -179), 0.6 - 2.0 / 30 * 0.4, 1e-3);
  NEAR(waveLabLoudness(-179, 179), 0.6 + 2.0 / 30 * 0.4, 1e-3);
  NEAR(waveLabAngleStep(10, 20, 0.5f), 15, 1e-4);
  NEAR(fabs(waveLabAngleStep(170, -170, 0.5f)), 180, 1e-3);
  NEAR(fabs(waveLabAngleStep(-170, 170, 0.5f)), 180, 1e-3);
  NEAR(waveLabAngleStep(175, -175, 1.0f), -175, 1e-3);
  for (int a = -180; a <= 180; a += 15) {
    for (int b = -180; b <= 180; b += 15) {
      const float r = waveLabAngleStep((float)a, (float)b, 0.3f);
      CHECK(r > -180.001f && r <= 180.001f);
    }
  }
}

// Turned toward portrait the loudness can't be read: it holds
static void testLoudReadable() {
  CHECK(waveLabLoudReadable(0));
  CHECK(waveLabLoudReadable(70));
  CHECK(waveLabLoudReadable(-70));
  CHECK(!waveLabLoudReadable(71));
  CHECK(!waveLabLoudReadable(-75));
}

static std::string secondText(int pattern, float hz) {
  char out[24];
  waveLabSecondText(WAVE_LAB_PATTERNS[pattern], hz, out, sizeof(out));
  return out;
}

// The second tone: the octave and the fifth by their pitch, the beat by how far apart the two are
static void testSecondText() {
  CHECK(secondText(0, 440) == "");
  CHECK(secondText(4, 440) == "+ 880 Hz");
  CHECK(secondText(5, 440) == "+ 660 Hz");
  CHECK(secondText(6, 440) == "+ 4 Hz");
  CHECK(secondText(4, 880) == "+ 1760 Hz");
}

static std::string lengthText(float hz, bool german) {
  char out[24];
  waveLabWavelengthText(hz, german, out, sizeof(out));
  return out;
}

// One wavelength in cm below a metre, else in m, with the language's decimal mark. Rounded first, then the unit
// chosen, so 343.5 Hz is "1,00 m", never "100 cm"
static void testWavelengthText() {
  CHECK(lengthText(440, true) == "\xCE\xBB 78 cm");
  CHECK(lengthText(880, true) == "\xCE\xBB 39 cm");
  CHECK(lengthText(220, true) == "\xCE\xBB 1,56 m");
  CHECK(lengthText(220, false) == "\xCE\xBB 1.56 m");
  CHECK(lengthText(343.5f, true) == "\xCE\xBB 1,00 m");
  CHECK(lengthText(344.5f, false) == "\xCE\xBB 1.00 m");
  CHECK(lengthText(346.0f, false) == "\xCE\xBB 99 cm");
}

// The speaker on screen shows what you hear, as music's does: 0 to 3 sound waves from Zitro's volume times the
// tilt's loudness; none with the sound off or the volume at 0 (Matthias, 2026-11-22: "Speaker should reflect the volume")
static void testSpeakerWaves() {
  CHECK(waveLabSpeakerWaves(false, 210, 1.0f) == 0);
  CHECK(waveLabSpeakerWaves(true, 0, 1.0f) == 0);
  CHECK(waveLabSpeakerWaves(true, 210, 1.0f) == 3);
  CHECK(waveLabSpeakerWaves(true, 210, 0.6f) == 2);
  CHECK(waveLabSpeakerWaves(true, 210, 0.2f) == 1);
  CHECK(waveLabSpeakerWaves(true, 191, 1.0f) == 3);  // The StickS3's highest
  CHECK(waveLabSpeakerWaves(true, 40, 1.0f) == 1);
  CHECK(waveLabSpeakerWaves(true, 1, 0.2f) == 1);    // Faint, but sounding
  CHECK(waveLabSpeakerWaves(true, 300, 2.0f) == 3);  // Out of range: clamped
  CHECK(waveLabSpeakerWaves(true, -5, 1.0f) == 0);
}

// tone(hz, ms) plays whole cycles, as many as its own formula gives; 0 would mean a tone without end
static void testToneCycles() {
  CHECK(waveLabToneCycles(50, 440.0f) == 22);
  CHECK(waveLabToneCycles(50, 439.9f) == 21);  // A shade lower: a whole cycle shorter
  CHECK(waveLabToneCycles(50, 220.0f) == 11);
  CHECK(waveLabToneCycles(1, 440.0f) == 0);
}

// The held sound: each frame sends one long piece that takes over from the playing one where its wave stands, so
// the sound can't run out between frames (Matthias, 2026-11-23: "it clicks in the first mode when tilting a
// little": frames 40 to 60 ms apart, pieces of at most 50 ms with one queued behind). The piece outlasts any frame
// gap short of a stall, yet a dark screen, which stops the frames without a word to the lab, is quiet within a
// quarter of a second.
static void testPieces() {
  CHECK(WAVE_LAB_PIECE_MS >= 3 * 60);
  CHECK(WAVE_LAB_PIECE_MS <= 250);
  for (float hz = 50; hz <= 2000; hz += 0.37f) {
    CHECK(waveLabToneCycles(WAVE_LAB_PIECE_MS, hz) >= 1);
    // A new shape: the old one plays to the end of a cycle and the new one follows from the start of its own.
    // After the cycle it's cut into, the old one has to last two speaker passes (5.3 ms each), so the new piece
    // is queued before it ends.
    if (hz < WAVE_LAB_MIN_HZ) continue;
    const uint32_t cycles = waveLabToneCycles(WAVE_LAB_FINISH_MS, hz);
    CHECK(cycles >= 2);
    CHECK((cycles - 1) * 1000.0f / hz >= 10.6f);
  }
  CHECK(WAVE_LAB_FINISH_MS <= 40);  // A new shape comes at once
}

// What a speaker channel does in a frame, from what it plays and what the scene wants
static void testSoundStep() {
  CHECK(waveLabSoundStep(false, WAVE_SHAPE_SINE, false, WAVE_SHAPE_SINE) == WAVE_LAB_STEP_NONE);
  CHECK(waveLabSoundStep(true, WAVE_SHAPE_SINE, false, WAVE_SHAPE_SINE) == WAVE_LAB_STEP_STOP);
  CHECK(waveLabSoundStep(false, WAVE_SHAPE_SAW, true, WAVE_SHAPE_SINE) == WAVE_LAB_STEP_START);
  CHECK(waveLabSoundStep(true, WAVE_SHAPE_SINE, true, WAVE_SHAPE_SINE) == WAVE_LAB_STEP_RETUNE);
  CHECK(waveLabSoundStep(true, WAVE_SHAPE_SQUARE, true, WAVE_SHAPE_SQUARE) == WAVE_LAB_STEP_RETUNE);
  CHECK(waveLabSoundStep(true, WAVE_SHAPE_SINE, true, WAVE_SHAPE_TRIANGLE) == WAVE_LAB_STEP_SWITCH);
  CHECK(waveLabSoundStep(true, WAVE_SHAPE_SAW, true, WAVE_SHAPE_SINE) == WAVE_LAB_STEP_SWITCH);
}

int main() {
  testPitch();
  testLoudness();
  testTiltAngles();
  testShapes();
  testTables();
  testPatterns();
  testNotes();
  testAwake();
  testChannelVolume();
  testLoudIgnoresPitchTilt();
  testLoudAcrossTheSeam();
  testLoudReadable();
  testSecondText();
  testWavelengthText();
  testSpeakerWaves();
  testToneCycles();
  testPieces();
  testSoundStep();
  return finish("wave lab");
}
