// School physics scenes: computed and animated on the device, no network. See physics_core.h.
#include "physics_draw.h"

static int physicsSceneIndex = -1;
static uint8_t physicsParam = 0;  // Kept while the view closes and opens again: Button C and back
static bool physicsGerman = true;
static PhysicsState* physicsState = nullptr;
static const int PHYSICS_SPEAKER_CHANNEL = 1;  // As music; reading aloud and the alert use channel 0
static bool physicsSounded = false;

// The wave lab's held sound: each frame sends a piece of whole wave cycles that takes over from the playing one,
// long enough to last until the next frame (WAVE_LAB_PIECE_MS), so it runs out by itself soon after frames stop.
// Lower tone on channel 1, upper on channel 2 (unused elsewhere). Design: docs/superpowers/specs/2026-11-16-wave-lab-design.md
static const int PHYSICS_LAB_CHANNEL_HIGH = 2;
static const uint32_t PHYSICS_LAB_GAP_MS = 1000;  // Frames this far apart: the screen was dark or an alert came
static const float PHYSICS_LAB_MOVE_DEG = 2.0f;   // A tilt that counts as playing, for staying awake
// The accelerometer gives "up" in the screen's axes (x right, y up, z towards you, gyroToScreen). Turning the
// right edge down makes up's x negative; leaning the top edge away makes its z positive.
static const float PHYSICS_LAB_PITCH_SIGN = -1.0f;  // Right edge down: higher
static const float PHYSICS_LAB_LOUD_SIGN = 1.0f;    // Top edge away: louder
static bool physicsLabSounding[2] = {false, false};
static uint8_t physicsLabShape[2] = {WAVE_SHAPE_SINE, WAVE_SHAPE_SINE};  // The table each channel plays
static bool physicsLabVolumesSaved = false;
static uint8_t physicsLabVolumes[2] = {255, 255};
static uint32_t physicsLabLastFrame = 0, physicsLabLastMove = 0, physicsLabLastPress = 0;
static float physicsLabNotedPitch = 0, physicsLabNotedLoud = 0;

static bool physicsIsLab() { return physicsState && physicsState->scene == PHYSICS_WAVE_LAB; }

// Silences the lab's channels and gives them back the volume they had
static void physicsLabQuiet() {
  for (int i = 0; i < 2; i++) {
    if (physicsLabSounding[i]) M5.Speaker.stop(PHYSICS_SPEAKER_CHANNEL + i);
    physicsLabSounding[i] = false;
  }
  if (physicsLabVolumesSaved) {
    M5.Speaker.setChannelVolume(PHYSICS_SPEAKER_CHANNEL, physicsLabVolumes[0]);
    M5.Speaker.setChannelVolume(PHYSICS_LAB_CHANNEL_HIGH, physicsLabVolumes[1]);
    physicsLabVolumesSaved = false;
  }
}

static void physicsEnd() {
  // Each tone ends by itself within two frames; this stops the last one at once
  if (physicsSounded) M5.Speaker.stop(PHYSICS_SPEAKER_CHANNEL);
  physicsSounded = false;
  physicsLabQuiet();
  free(physicsState);
  physicsState = nullptr;
}

// The label must always name what's on screen, and an atom's name changes with Button B
static void physicsSetLabel() {
  const PhysicsSceneInfo& info = PHYSICS_SCENES[physicsSceneIndex];
  if (info.scene == PHYSICS_ATOM) {
    const PhysicsElement& element = PHYSICS_ELEMENTS[physicsParam];
    modelLabel = physicsGerman ? element.de : element.en;
    modelSourceName = "PubChem";
  } else {
    modelLabel = physicsGerman ? info.de : info.en;
    modelSourceName = physicsGerman ? "berechnet" : "computed";
  }
}

static bool physicsFetch(const char* query) {
  int element = 0;
  if (!physicsParseQuery(query, physicsSceneIndex, element)) return false;
  physicsParam = element > 0 ? (uint8_t)(element - 1) : 0;
  Preferences settings;
  settings.begin("settings", true);
  physicsGerman = settings.getString("language", "DE") != "EN";
  settings.end();
  physicsSetLabel();
  return true;
}

static bool physicsBegin(M5Canvas& c) {
  (void)c;
  if (physicsSceneIndex < 0) return false;
  physicsEnd();
  physicsState = (PhysicsState*)ps_malloc(sizeof(PhysicsState));
  if (!physicsState) return false;
  memset(physicsState, 0, sizeof(PhysicsState));  // The wave lab's sound starts off
  physicsState->sceneIndex = (uint8_t)physicsSceneIndex;
  physicsState->param = physicsParam;
  physicsState->scene = PHYSICS_SCENES[physicsSceneIndex].scene;
  physicsState->german = physicsGerman;
  physicsState->big = FontManager::getFont16pt();
  physicsState->normal = FontManager::getFont12pt();
  physicsState->clockKnown = physicsDayFromClock((double)time(nullptr), physicsState->startDay);
  // Home from the settings, so seasons and the Sun's path are for home
  float homeLat, homeLon;
  homeLatLon(homeLat, homeLon);
  physicsState->homeLatitude = homeLat * PHYSICS_DEG;
  physicsState->homeLongitude = homeLon * PHYSICS_DEG;
  physicsState->random = (uint32_t)esp_random();
  physicsSceneReset(*physicsState);
  const uint32_t now = millis();
  physicsLabLastFrame = physicsLabLastMove = physicsLabLastPress = now;
  return true;
}

// The wave lab: the tilt in, before drawing. A wake since the last frame (however quick), a long gap between
// frames, or 5 minutes without a press, switch the sound off.
static void physicsLabBeforeFrame() {
  const uint32_t now = millis();
  const bool wokeSince = (int32_t)((uint32_t)timeWokeFromSleep - physicsLabLastFrame) > 0;
  if (physicsState->labSoundOn && (wokeSince || now - physicsLabLastFrame > PHYSICS_LAB_GAP_MS ||
                                   waveLabPressTimedOut(now, physicsLabLastPress))) {
    physicsWaveLabSoundSwitch(*physicsState, false);
  }
  physicsLabLastFrame = now;
  M5.Imu.update();
  const auto imu = M5.Imu.getImuData();
  float up[3];
  gyroToScreen(M5.Display.getRotation(), imu.accel.x, imu.accel.y, imu.accel.z, up);
  physicsState->tiltPitchDeg = PHYSICS_LAB_PITCH_SIGN * waveLabPitchTiltDeg(up[0], up[1], up[2]);
  physicsState->tiltLoudDeg = PHYSICS_LAB_LOUD_SIGN * waveLabLoudTiltDeg(up[0], up[1], up[2]);
  physicsState->deviceVolume = M5.Speaker.getVolume();  // The speaker symbol shows what you hear
  if (fabsf(physicsState->tiltPitchDeg - physicsLabNotedPitch) > PHYSICS_LAB_MOVE_DEG ||
      fabsf(physicsState->tiltLoudDeg - physicsLabNotedLoud) > PHYSICS_LAB_MOVE_DEG) {
    physicsLabNotedPitch = physicsState->tiltPitchDeg;
    physicsLabNotedLoud = physicsState->tiltLoudDeg;
    physicsLabLastMove = now;
  }
}

// The wave lab: plays what the scene wants, after drawing. Two tones share the loudness, so they never clip.
// Each frame one long piece takes over from the playing one where its wave stands (wave_lab_core.h): the sound
// follows the tilt and can't run out between frames.
static void physicsLabSoundOut() {
  const PhysicsLabSound& sound = physicsState->labSound;
  for (int i = 0; i < 2; i++) {
    const int channel = PHYSICS_SPEAKER_CHANNEL + i;
    const WaveLabSoundStep step =
        waveLabSoundStep(physicsLabSounding[i], physicsLabShape[i], i < sound.count && sound.hz[i] > 0, sound.shape);
    if (step == WAVE_LAB_STEP_NONE) continue;
    if (step == WAVE_LAB_STEP_STOP) {
      M5.Speaker.stop(channel);
      physicsLabSounding[i] = false;
      continue;
    }
    if (!physicsLabVolumesSaved) {
      physicsLabVolumes[0] = M5.Speaker.getChannelVolume(PHYSICS_SPEAKER_CHANNEL);
      physicsLabVolumes[1] = M5.Speaker.getChannelVolume(PHYSICS_LAB_CHANNEL_HIGH);
      physicsLabVolumesSaved = true;
    }
    M5.Speaker.begin();  // Sleep switches it off; returns at once if it's running
    const float share = sound.count == 2 ? 0.5f : 1.0f;
    M5.Speaker.setChannelVolume(channel, waveLabChannelVolume(sound.loudness, share));  // The mixer squares it
    const uint8_t* table = waveLabTable(sound.shape);
    if (step == WAVE_LAB_STEP_SWITCH) {
      // Taking over would start the new table from its beginning, wherever the wave stands, and click. So the old
      // shape plays on to the end of a cycle and the new one is queued behind it. The second call waits until the
      // speaker has taken the first: a few milliseconds.
      M5.Speaker.tone(sound.hz[i], WAVE_LAB_FINISH_MS, channel, true, waveLabTable(physicsLabShape[i]), WAVE_LAB_TABLE_LEN);
      M5.Speaker.tone(sound.hz[i], WAVE_LAB_PIECE_MS, channel, false, table, WAVE_LAB_TABLE_LEN);
    } else if (step == WAVE_LAB_STEP_START || M5.Speaker.isPlaying(channel) < 2) {
      // Not while a new shape still waits behind the old one's last cycles: it would take their place and jump
      M5.Speaker.tone(sound.hz[i], WAVE_LAB_PIECE_MS, channel, true, table, WAVE_LAB_TABLE_LEN);
    }
    physicsLabSounding[i] = true;
    physicsLabShape[i] = sound.shape;
  }
}

static void physicsFrame(M5Canvas& c, float dt) {
  if (!physicsState) return;
  const bool lab = physicsIsLab();
  if (lab) physicsLabBeforeFrame();
  physicsDrawFrame(c, *physicsState, dt);
  eduDrawFooter(c, 0);
  if (lab) physicsLabSoundOut();
  // A sound the scene asked for: a short tone at the device's volume, never waited for
  if (physicsState->toneMs) {
    M5.Speaker.begin();  // Sleep switches it off; returns at once if it's running
    M5.Speaker.tone(physicsState->toneHz, physicsState->toneMs, PHYSICS_SPEAKER_CHANNEL, true);
    physicsState->toneMs = 0;
    physicsSounded = true;
  }
}

static void physicsButtonB() {
  if (!physicsState) return;
  physicsSceneNextParam(*physicsState);
  physicsParam = physicsState->param;
  physicsSetLabel();
  physicsLabLastPress = millis();
}

// The wave lab's sound holds off sleep while the hand plays it, up to 5 minutes after the last press
static bool physicsKeepAwake() {
  return physicsIsLab() && physicsState->labSoundOn &&
         waveLabKeepAwake(millis(), physicsLabLastMove, physicsLabLastPress);
}

// B changes something in the scene, if it has more than one setting; C is the written answer, as by default.
// The wave lab has no C tab: its way back is the menu row "Text".
static void physicsHints(int& kindB, int& kindC) {
  kindB = (physicsState && PHYSICS_SCENES[physicsState->sceneIndex].paramCount > 1) ? HINT_NEXT : HINT_NONE;
  if (physicsIsLab()) kindC = HINT_NONE;
}

// The wave lab's own menu row: the sound on or off (view_menu.ino adds "Text" and "Schliessen")
static int physicsMenuRowCount() { return physicsIsLab() ? 1 : 0; }

static const char* physicsMenuRowLabel(int i, bool german) {
  (void)i;
  if (physicsIsLab() && physicsState->labSoundOn) return german ? "Ton aus" : "Sound off";
  return german ? "Ton an" : "Sound on";
}

static bool physicsMenuRowChoose(int i) {
  if (i != 0 || !physicsIsLab()) return false;
  physicsWaveLabSoundSwitch(*physicsState, !physicsState->labSoundOn);
  physicsLabLastPress = physicsLabLastMove = millis();
  return true;  // Stays open: "Ton aus" is right there
}

// Button C goes back to the text in every scene, so nobody gets stuck in one; in the wave lab it opens the lab's
// menu, whose row "Text" goes back (model_view.ino, modelButtonC()).
const EduFeature FEATURE_PHYSICS = {"physics",    "",           PHYSICS_PROMPT_RULE, false,          physicsFetch, nullptr,
                                    physicsBegin, physicsFrame, physicsEnd,          physicsButtonB, nullptr,      physicsKeepAwake,
                                    nullptr,      physicsHints, false,               physicsMenuRowCount,
                                    physicsMenuRowLabel,         physicsMenuRowChoose};
