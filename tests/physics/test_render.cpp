// Runs the real drawing code on a stand-in canvas: nothing may crash, no text may leave the
// screen or lack a glyph, in either language. With ZITRO_RENDER_DIR set it also writes pictures.
#include <algorithm>
#include <cstdlib>
#include <string>
#include "mock_canvas.h"
#include "test_common.h"
#include "../../sketch/sketch_jan12a/physics_draw.h"
#include "../../sketch/sketch_jan12a/solid_draw.h"

static M5Canvas canvas;
static const char* outDir = nullptr;

static void save(const std::string& name) {
  if (outDir) canvas.writePpm(std::string(outDir) + "/" + name + ".ppm", 2);
}

static SolidState solid;
static void renderSolids() {
  Mat3 r;
  for (int lang = 0; lang < 2; lang++) {
    for (int i = 0; i < SOLID_TABLE_SIZE; i++) {
      CHECK(solidBuild(i, solid.mesh));
      solid.color = SOLID_TABLE[i].color;
      for (int zoom = 0; zoom < 3; zoom++) {
        mat3Identity(r);
        mat3Rotate(r, 0.35f, 0.5f, 0.0f);
        for (int turn = 0; turn < 40; turn++) {
          canvas.context = std::string("solid ") + SOLID_TABLE[i].id;
          solidDrawFrame(canvas, solid, r, (uint8_t)zoom, lang == 0, &Roboto_Regular14pt8b, &Roboto_Regular10pt8b, 240, 117);
          canvas.footerStandIn(lang == 0 ? SOLID_TABLE[i].de : SOLID_TABLE[i].en);
          if (turn == 0 && zoom == 0 && lang == 0) save(std::string("solid_") + SOLID_TABLE[i].id);
          mat3Rotate(r, 0.2f, 0.37f, 0.05f);
        }
      }
    }
  }
}

static PhysicsState state;
static int tones = 0;

struct Home { const char* name; float latitude, longitude; };
// Home as the firmware sets it now, and places the home city setting may bring: south, the tropics, the far north
static const Home HOMES[] = {{"", 47.0f, 8.5f}, {"sydney", -33.9f, 151.2f}, {"singapore", 1.3f, 103.8f}, {"tromso", 69.6f, 18.9f}};

static bool homeMatters(PhysicsSceneId scene) { return scene == PHYSICS_SEASONS || scene == PHYSICS_SUNPATH; }

// Every scene, every setting, both languages: half a minute of simulated play at 25 frames per second
static void renderScenes() {
  for (const Home& home : HOMES)
  for (int lang = 0; lang < 2; lang++) {
    for (int i = 0; i < PHYSICS_SCENE_COUNT; i++) {
      const PhysicsSceneInfo& info = PHYSICS_SCENES[i];
      if (home.name[0] && !homeMatters(info.scene)) continue;
      memset(&state, 0, sizeof(state));
      state.sceneIndex = (uint8_t)i;
      state.scene = info.scene;
      state.german = lang == 0;
      state.big = &Roboto_Regular14pt8b;
      state.normal = &Roboto_Regular10pt8b;
      // English runs without a clock, German with one: both must draw
      state.clockKnown = physicsDayFromClock(lang == 0 ? 1793000000.0 : 0.0, state.startDay);
      state.homeLatitude = home.latitude * PHYSICS_DEG;
      state.homeLongitude = home.longitude * PHYSICS_DEG;
      physicsSceneReset(state);
      for (int p = 0; p < info.paramCount; p++) {
        char name[64];
        canvas.context = std::string("physics ") + info.id + (lang == 0 ? " DE" : " EN") + " setting " + std::to_string(p) +
                         (home.name[0] ? std::string(" ") + home.name : std::string());
        physicsDrawFrame(canvas, state, 0.0f);  // The first frame has no time step
        for (int frame = 0; frame < 750; frame++) {
          // Frame times as on the stick: mostly steady, sometimes a slow one
          physicsDrawFrame(canvas, state, frame % 40 == 7 ? 0.1f : 0.04f);
          // Sound only where the scene is about it, never longer than two frames, never too high to hear
          if (state.toneMs) {
            CHECK(info.scene != PHYSICS_WAVE_LAB);  // The lab holds its sound through labSound instead
            CHECK(info.scene == PHYSICS_DOPPLER || info.scene == PHYSICS_HALF_LIFE);
            CHECK(state.toneMs <= 80);
            CHECK(state.toneHz >= 100 && state.toneHz <= 5000);
            tones++;
            state.toneMs = 0;  // As the device does once it has played it
          }
          if (!state.labSoundOn) CHECK(state.labSound.count == 0);  // A held sound only once switched on
          const bool wanted = info.scene != PHYSICS_ATOM || p == 0 || p == 5 || p == 10 || p == 25 || p == 35;
          if (lang == 0 && wanted && (frame == 25 || frame == 110)) {
            canvas.footerStandIn(info.scene == PHYSICS_ATOM ? PHYSICS_ELEMENTS[p].de : info.de);
            snprintf(name, sizeof(name), "scene_%02d_%s%s%s_%d_%03d", i, info.id, home.name[0] ? "_" : "", home.name, p, frame);
            save(name);
          }
        }
        physicsSceneNextParam(state);
      }
      CHECK(state.param == 0);  // Button B comes back around to the start
    }
  }
}

// The wave lab at every pattern, tilt and loudness, with its sound off and on: the text fits, the held sound is
// what the screen shows and nothing else
static void renderWaveLab() {
  int lab = -1;
  for (int i = 0; i < PHYSICS_SCENE_COUNT; i++) {
    if (PHYSICS_SCENES[i].scene == PHYSICS_WAVE_LAB) lab = i;
  }
  CHECK(lab >= 0);
  if (lab < 0) return;
  const float pitches[] = {-60, -40, -20, 0, 20, 40, 60};
  const float louds[] = {-45, 0, 45};
  for (int lang = 0; lang < 2; lang++)
  for (int p = 0; p < WAVE_LAB_PATTERN_COUNT; p++)
  for (float pitch : pitches)
  for (float loud : louds)
  for (int on = 0; on < 2; on++) {
    memset(&state, 0, sizeof(state));
    state.sceneIndex = (uint8_t)lab;
    state.scene = PHYSICS_WAVE_LAB;
    state.param = (uint8_t)p;
    state.german = lang == 0;
    state.big = &Roboto_Regular14pt8b;
    state.normal = &Roboto_Regular10pt8b;
    physicsSceneReset(state);
    physicsWaveLabSoundSwitch(state, on == 1);
    state.deviceVolume = (uint8_t)(p % 2 ? 210 : (lang == 0 ? 191 : 0));  // Plus2's default, the StickS3's top, silent
    state.tiltPitchDeg = pitch;
    state.tiltLoudDeg = loud;
    canvas.context = std::string("physics wave-lab ") + (lang == 0 ? "DE" : "EN") + " pattern " + std::to_string(p) +
                     " pitch " + std::to_string((int)pitch) + " loud " + std::to_string((int)loud) + (on ? " on" : " off");
    physicsDrawFrame(canvas, state, 0.0f);
    for (int frame = 0; frame < 60; frame++) {
      physicsDrawFrame(canvas, state, 0.04f);
      CHECK(state.toneMs == 0);
    }
    canvas.footerStandIn(lang == 0 ? PHYSICS_SCENES[lab].de : PHYSICS_SCENES[lab].en);
    if (!on) {
      CHECK(state.labSound.count == 0);
    } else {
      CHECK(state.labSound.count == (p < 4 ? 1 : 2));
      CHECK(state.labSound.hz[0] >= 219.9f && state.labSound.hz[0] <= 880.1f);
      CHECK(state.labSound.hz[1] == 0 || (state.labSound.hz[1] >= 219.9f && state.labSound.hz[1] <= 1760.1f));
      CHECK(state.labSound.loudness >= 0.2f - 1e-5f && state.labSound.loudness <= 1.0f + 1e-5f);
      CHECK(state.labSound.shape < WAVE_SHAPE_COUNT);
    }
    if (lang == 0 && on && pitch == 0 && loud == 0) save("wavelab_" + std::to_string(p));
  }
}

// Turned toward portrait for the highest note, the lean can't be read: the loudness holds where it was. Upside down,
// crossing 180 / -180 moves it 2 degrees, not to the far end.
static void checkWaveLabLoudnessHolds() {
  int lab = -1;
  for (int i = 0; i < PHYSICS_SCENE_COUNT; i++) {
    if (PHYSICS_SCENES[i].scene == PHYSICS_WAVE_LAB) lab = i;
  }
  if (lab < 0) return;
  memset(&state, 0, sizeof(state));
  state.sceneIndex = (uint8_t)lab;
  state.scene = PHYSICS_WAVE_LAB;
  state.german = true;
  state.big = &Roboto_Regular14pt8b;
  state.normal = &Roboto_Regular10pt8b;
  physicsSceneReset(state);
  physicsWaveLabSoundSwitch(state, true);
  canvas.context = "physics wave-lab loudness holds";
  physicsDrawFrame(canvas, state, 0.0f);
  for (int f = 0; f < 30; f++) physicsDrawFrame(canvas, state, 0.04f);
  NEAR(state.labSound.loudness, 0.6, 1e-3);
  state.tiltPitchDeg = 80;  // Past the readable range: the wobbling lean must not count
  state.tiltLoudDeg = 45;
  for (int f = 0; f < 30; f++) physicsDrawFrame(canvas, state, 0.04f);
  NEAR(state.labSound.loudness, 0.6, 1e-3);

  memset(&state, 0, sizeof(state));
  state.sceneIndex = (uint8_t)lab;
  state.scene = PHYSICS_WAVE_LAB;
  state.german = true;
  state.big = &Roboto_Regular14pt8b;
  state.normal = &Roboto_Regular10pt8b;
  physicsSceneReset(state);
  physicsWaveLabSoundSwitch(state, true);
  state.tiltLoudDeg = 179;  // Upside down
  physicsDrawFrame(canvas, state, 0.0f);
  state.tiltLoudDeg = -179;
  for (int f = 0; f < 30; f++) {
    physicsDrawFrame(canvas, state, 0.04f);
    CHECK(state.labSound.loudness > 0.55f && state.labSound.loudness < 0.65f);
  }
}

int main() {
  outDir = getenv("ZITRO_RENDER_DIR");
  renderSolids();
  renderScenes();
  renderWaveLab();
  checkWaveLabLoudnessHolds();
  CHECK(canvas.textProblems == 0);
  CHECK(tones > 0);
  return finish("render");
}
