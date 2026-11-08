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
            CHECK(info.scene == PHYSICS_DOPPLER || info.scene == PHYSICS_HALF_LIFE);
            CHECK(state.toneMs <= 80);
            CHECK(state.toneHz >= 100 && state.toneHz <= 5000);
            tones++;
            state.toneMs = 0;  // As the device does once it has played it
          }
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

int main() {
  outDir = getenv("ZITRO_RENDER_DIR");
  renderSolids();
  renderScenes();
  CHECK(canvas.textProblems == 0);
  CHECK(tones > 0);
  return finish("render");
}
