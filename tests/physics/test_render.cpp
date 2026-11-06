// Runs the real drawing code on a stand-in canvas: nothing may crash, no text may leave the
// screen or lack a glyph, in either language. With ZITRO_RENDER_DIR set it also writes pictures.
#include <algorithm>
#include <cstdlib>
#include <string>
#include "mock_canvas.h"
#include "test_common.h"
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

int main() {
  outDir = getenv("ZITRO_RENDER_DIR");
  renderSolids();
  CHECK(canvas.textProblems == 0);
  return finish("render");
}
