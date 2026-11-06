// Draws a solid onto the canvas: lit faces, far side hidden, nearest on top. Written against any
// canvas type, so tests/physics draws the same pictures on the host.
#pragma once

#include <stdio.h>

#include "model_core.h"
#include "solid_core.h"

static const float SOLID_CAMERA = 3.0f;  // Distance from the middle, in solid radii
static const int SOLID_LEFT_MIDDLE = 72;   // Polyhedra leave room for the counts
static const int SOLID_COUNTS_LEFT = 130;
static const float SOLID_ZOOMS[3] = {1.0f, 1.5f, 2.2f};

struct SolidState {
  SolidMesh mesh;
  uint16_t color;
  int16_t sx[SOLID_MAX_VERTICES], sy[SOLID_MAX_VERTICES];
  bool cornerSeen[SOLID_MAX_VERTICES];
  float depth[SOLID_MAX_FACES];
  float bright[SOLID_MAX_FACES];
  uint8_t order[SOLID_MAX_FACES];
};

// One row of the counts: the number big, the word next to it
template <class C>
void solidDrawCount(C& c, const GFXfont* big, const GFXfont* normal, int value, const char* word, int y, uint16_t color) {
  char number[8];
  snprintf(number, sizeof(number), "%d", value);
  c.setTextColor(color);
  c.setFreeFont(big);
  c.setTextDatum(textdatum_t::baseline_right);
  c.drawString(number, SOLID_COUNTS_LEFT + 30, y);
  c.setFreeFont(normal);
  c.setTextDatum(textdatum_t::baseline_left);
  c.drawString(word, SOLID_COUNTS_LEFT + 36, y);
}

template <class C>
void solidDrawFrame(C& c, SolidState& s, const Mat3& rotation, uint8_t zoomStep, bool german, const GFXfont* big,
                    const GFXfont* normal, int width, int viewHeight) {
  const SolidMesh& m = s.mesh;
  c.fillSprite((uint16_t)0x0000);
  const int cx = m.curved ? width / 2 : SOLID_LEFT_MIDDLE;
  const int cy = viewHeight / 2;
  const float scale = viewHeight * 0.42f * SOLID_ZOOMS[zoomStep < 3 ? zoomStep : 0];

  float p[3];
  for (uint16_t i = 0; i < m.vertexCount; i++) {
    rotatePoint(rotation, m.v[i][0], m.v[i][1], m.v[i][2], p);
    const float perspective = SOLID_CAMERA / (SOLID_CAMERA - p[2]);
    s.sx[i] = (int16_t)(cx + p[0] * scale * perspective);
    s.sy[i] = (int16_t)(cy - p[1] * scale * perspective);
    s.cornerSeen[i] = false;
  }

  // Faces turned towards the camera, seen from the camera's real position
  uint16_t visible = 0;
  for (uint16_t i = 0; i < m.faceCount; i++) {
    const SolidFace& f = m.f[i];
    float n[3], mid[3];
    rotatePoint(rotation, f.nx, f.ny, f.nz, n);
    rotatePoint(rotation, f.cx, f.cy, f.cz, mid);
    if (n[0] * -mid[0] + n[1] * -mid[1] + n[2] * (SOLID_CAMERA - mid[2]) <= 0) continue;
    s.depth[i] = mid[2];
    s.bright[i] = solidBrightness(n[0], n[1], n[2]);
    s.order[visible++] = (uint8_t)i;
  }
  sortByDepth(s.depth, s.order, visible);

  const uint16_t edgeColor = (uint16_t)0x0000;
  for (uint16_t n = 0; n < visible; n++) {
    const uint8_t i = s.order[n];
    const SolidFace& f = m.f[i];
    const uint8_t* corners = m.indices + f.start;
    const uint16_t color = solidShade565(s.color, s.bright[i]);
    for (uint8_t k = 1; k + 1 < f.count; k++) {
      c.fillTriangle(s.sx[corners[0]], s.sy[corners[0]], s.sx[corners[k]], s.sy[corners[k]], s.sx[corners[k + 1]],
                     s.sy[corners[k + 1]], color);
    }
    if (!m.curved) {
      // Real edges only: a curved solid has no lines between its small faces
      for (uint8_t k = 0; k < f.count; k++) {
        const uint8_t a = corners[k], b = corners[(k + 1) % f.count];
        c.drawLine(s.sx[a], s.sy[a], s.sx[b], s.sy[b], edgeColor);
        s.cornerSeen[a] = true;
      }
    }
  }
  if (m.curved) return;

  for (uint16_t i = 0; i < m.vertexCount; i++) {
    if (s.cornerSeen[i]) c.fillCircle(s.sx[i], s.sy[i], 2, (uint16_t)0xFFFF);
  }
  // Numbers stay readable over a zoomed solid
  c.fillRect(SOLID_COUNTS_LEFT - 2, 0, width - SOLID_COUNTS_LEFT + 2, viewHeight, (uint16_t)0x0000);
  solidDrawCount(c, big, normal, m.faces, german ? "Fl\xC3\xA4" "chen" : "Faces", 32, solidShade565(s.color, 1.0f));
  solidDrawCount(c, big, normal, m.edges, german ? "Kanten" : "Edges", 66, (uint16_t)0xC618);
  solidDrawCount(c, big, normal, m.vertices, german ? "Ecken" : "Vertices", 100, (uint16_t)0xFFFF);
}
