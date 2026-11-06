// Host preview: draws a model file with the device's own pipeline at the real 240x135, as a PPM.
// Usage: render_bin <file.bin> <id> <out.ppm> <xAngle> <yAngle> <zoomStep> [cut]
// With "cut" the angles are added to the view's start pose, and the legend lines are printed as "legend <rgb565> <de>|<en>".
// tests/anatomy/preview.py turns these into a contact sheet to check by eye.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "../../sketch/sketch_jan12a/anatomy_core.h"
#include "../../sketch/sketch_jan12a/anatomy_view.h"

static const int W = 240, H = 135;
static uint16_t pixels[H][W];

// Same scanline idea as the display library's fillTriangle
static void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y1 > y2) { int t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  for (int y = y0; y <= y2; y++) {
    if (y < 0 || y >= H) continue;
    float a = y2 != y0 ? x0 + (float)(x2 - x0) * (y - y0) / (y2 - y0) : x0;
    float b;
    if (y < y1) b = y1 != y0 ? x0 + (float)(x1 - x0) * (y - y0) / (y1 - y0) : x0;
    else b = y2 != y1 ? x1 + (float)(x2 - x1) * (y - y1) / (y2 - y1) : x1;
    if (a > b) { float t = a; a = b; b = t; }
    for (int x = (int)a; x <= (int)b; x++) {
      if (x >= 0 && x < W) pixels[y][x] = color;
    }
  }
}

int main(int argc, char** argv) {
  if (argc < 7) return 2;
  FILE* f = fopen(argv[1], "rb");
  if (!f) return 3;
  std::vector<uint8_t> raw(ANATOMY_BODY_LIMIT + 8);
  // Host allocations are aligned, as ps_malloc's are on the device
  const size_t n = fread(raw.data(), 1, raw.size(), f);
  fclose(f);
  const bool cut = argc > 7 && strcmp(argv[7], "cut") == 0;
  AnatomyModel m;
  if (!anatomyParse(raw.data(), n, argv[2], cut, m)) {
    fprintf(stderr, "%s: rejected\n", argv[1]);
    return 4;
  }
  std::vector<int8_t> normals(m.triangleCount * 3);
  anatomyComputeNormals(m, normals.data());
  uint16_t shades[ANATOMY_MAX_PALETTE * ANATOMY_SHADES];
  anatomyBuildShades(m, shades);
  std::vector<int16_t> sx(m.vertexCount), sy(m.vertexCount), depth(m.vertexCount);
  std::vector<uint16_t> next(m.triangleCount), heads(ANATOMY_DEPTH_BUCKETS);
  AnatomyWork w = {sx.data(), sy.data(), depth.data(), next.data(), heads.data()};

  Mat3 rotation;
  mat3Identity(rotation);
  if (cut) anatomyCutStart(rotation);
  mat3Rotate(rotation, (float)atof(argv[4]), (float)atof(argv[5]), 0);
  if (cut) anatomyClampCut(rotation);
  const int zoom = atoi(argv[6]);
  const int viewH = H - ANATOMY_VIEW_FOOTER;
  const uint16_t drawn = anatomyRender(m, normals.data(), shades, rotation, anatomyViewScaleFor(viewH, m.legendCount, zoom), ANATOMY_VIEW_CAMERA,
                                       anatomyViewCentreX(W, m.legendCount, zoom), viewH / 2, W, viewH, w,
                                       [](int a, int b, int c, int d, int e, int g, uint16_t color) {
                                         fillTriangle(a, b, c, d, e, g, color);
                                       });
  printf("%s tris=%d verts=%d drawn=%d\n", argv[2], m.triangleCount, m.vertexCount, drawn);
  if (anatomyViewShowsLegend(m.legendCount, zoom)) {
    printf("top %d %d\n", anatomyLegendTop(viewH, m.legendCount, (m.flags & ANATOMY_FLAG_SCHEMATIC) != 0), (m.flags & ANATOMY_FLAG_SCHEMATIC) != 0);
    for (int i = 0; i < m.legendCount; i++) {
      const uint8_t color = anatomyLegendColor(m, i);
      printf("legend %d %s|%s\n", color == ANATOMY_LEGEND_NOTE ? -1 : m.palette[color], anatomyLegendText(m, i, true), anatomyLegendText(m, i, false));
    }
  }
  f = fopen(argv[3], "wb");
  fprintf(f, "P6 %d %d 255\n", W, H);
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      const uint16_t p = pixels[y][x];
      const uint8_t rgb[3] = {(uint8_t)((p >> 11) << 3), (uint8_t)(((p >> 5) & 0x3F) << 2), (uint8_t)((p & 0x1F) << 3)};
      fwrite(rgb, 1, 3, f);
    }
  }
  fclose(f);
  return 0;
}
