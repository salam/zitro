// Pure logic of the anatomy view: closed model list, file check, the two views, projection, sorting, flat light.
// Plain C++ with no Arduino includes, tested on the host: tests/anatomy/run.sh
//
// File "ZAN1", little endian, made by scripts/make_anatomy_data.py:
//   0  char[4]  magic "ZAN1"
//   4  u8       flags (bit 0: schematic, built from textbook structure and not measured data; bit 1: cut open)
//   5  u8       palette entries, 1 to 8
//   6  u16      vertices
//   8  u16      triangles
//   10 u16      cut open: first filled triangle, 0 if none. Otherwise 0
//   12 char[16] id
//   28 char[24] German label
//   52 char[24] English label
//   76 char[24] source, short form
//   100 u16[8]  palette, RGB565
//   116 i16[3]  per vertex: x right, y up, z towards the viewer; radius 1 is 32767
//   ... u16[3]  per triangle: counter-clockwise seen from outside
//   ... u8      per triangle: palette index
//   ... cut open only: u8 legend lines, 1 to 8, then per line u8 palette index (0xFF: a note with no colour),
//       char[20] German, char[20] English, ASCII only
//   ... u32     CRC-32 of everything before it
//
// A cut file holds everything behind the plane z = 0, seen from +z. Filled triangles lie on that plane, so they're
// in front of everything else in every allowed view and are drawn last, in file order.
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "model_core.h"

#define ANATOMY_MAX_VERTICES 2500
#define ANATOMY_MAX_TRIANGLES 3500
#define ANATOMY_MAX_PALETTE 8
#define ANATOMY_MAX_LEGEND 8
#define ANATOMY_LEGEND_NOTE 0xFF
#define ANATOMY_LEGEND_TEXT 20
#define ANATOMY_LEGEND_SIZE (1 + 2 * ANATOMY_LEGEND_TEXT)
#define ANATOMY_FLAG_SCHEMATIC 0x01
#define ANATOMY_FLAG_CUT 0x02
#define ANATOMY_HEADER_SIZE 116
#define ANATOMY_BODY_LIMIT \
  (ANATOMY_HEADER_SIZE + 6 * ANATOMY_MAX_VERTICES + 7 * ANATOMY_MAX_TRIANGLES + 1 + ANATOMY_MAX_LEGEND * ANATOMY_LEGEND_SIZE + 4)
#define ANATOMY_DEPTH_BUCKETS 256
#define ANATOMY_SHADES 16
#define ANATOMY_FILL_SHADE 13
#define ANATOMY_NO_TRIANGLE 0xFFFF

// The closed list: file name on the server, and what the AI may ask for. Nothing else reaches a URL.
static const char* const ANATOMY_IDS[] = {"heart", "brain", "lungs", "stomach", "liver", "kidney", "skeleton", "skull",
                                          "eye", "hand", "animal-cell", "plant-cell", "red-blood-cell", "neuron"};
static const int ANATOMY_ID_COUNT = sizeof(ANATOMY_IDS) / sizeof(ANATOMY_IDS[0]);

// Which views exist for each id, in the same order. An organ has a cut only if the sources really have its inside.
// A cell is only a drawing, and what matters is inside, so its one file is already cut open.
enum AnatomyKind : uint8_t { ANATOMY_OUTSIDE_ONLY, ANATOMY_OUTSIDE_AND_CUT, ANATOMY_CUT_ONLY };
static const AnatomyKind ANATOMY_KINDS[] = {
    ANATOMY_OUTSIDE_AND_CUT,  // heart
    ANATOMY_OUTSIDE_AND_CUT,  // brain
    ANATOMY_OUTSIDE_AND_CUT,  // lungs
    ANATOMY_OUTSIDE_ONLY,     // stomach: one closed outer surface, nothing inside it in any source
    ANATOMY_OUTSIDE_AND_CUT,  // liver
    ANATOMY_OUTSIDE_AND_CUT,  // kidney
    ANATOMY_OUTSIDE_ONLY,     // skeleton
    ANATOMY_OUTSIDE_AND_CUT,  // skull
    ANATOMY_OUTSIDE_AND_CUT,  // eye
    ANATOMY_OUTSIDE_ONLY,     // hand: the bones already are its inside, and nothing lies within them
    ANATOMY_CUT_ONLY,         // animal-cell
    ANATOMY_CUT_ONLY,         // plant-cell
    ANATOMY_CUT_ONLY,         // red-blood-cell
    ANATOMY_CUT_ONLY,         // neuron
};
static_assert(sizeof(ANATOMY_KINDS) / sizeof(ANATOMY_KINDS[0]) == ANATOMY_ID_COUNT, "One kind per id");

static const char ANATOMY_INSIDE_SUFFIX[] = "-inside";

struct AnatomyModel {
  uint8_t flags, paletteCount;
  uint16_t vertexCount, triangleCount;
  uint16_t frontFrom;  // Cut open: first filled triangle, 0 if none
  uint8_t legendCount;
  const char* id;
  const char* labelDe;
  const char* labelEn;
  const char* source;
  uint16_t palette[ANATOMY_MAX_PALETTE];
  const int16_t* vertices;    // 3 per vertex
  const uint16_t* triangles;  // 3 per triangle
  const uint8_t* colors;      // 1 per triangle
  const uint8_t* legend;      // legendCount lines of ANATOMY_LEGEND_SIZE; never aligned
};

// Working memory for one frame
struct AnatomyWork {
  int16_t* sx;       // Per vertex: screen position
  int16_t* sy;
  int16_t* depth;    // Per vertex: towards the viewer, 1024 is radius 1
  uint16_t* next;    // Per triangle: next one in the same depth bucket
  uint16_t* heads;   // ANATOMY_DEPTH_BUCKETS
};

inline int32_t anatomyMin3(int32_t a, int32_t b, int32_t c) {
  const int32_t m = a < b ? a : b;
  return m < c ? m : c;
}

inline int32_t anatomyMax3(int32_t a, int32_t b, int32_t c) {
  const int32_t m = a > b ? a : b;
  return m > c ? m : c;
}

// Index in the closed list, or -1. Case doesn't matter; nothing else is forgiven.
inline int anatomyFindId(const char* query) {
  if (!query) return -1;
  for (int i = 0; i < ANATOMY_ID_COUNT; i++) {
    const char* a = ANATOMY_IDS[i];
    const char* b = query;
    while (*a && *b) {
      const char lower = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
      if (*a != lower) break;
      a++;
      b++;
    }
    if (!*a && !*b) return i;
  }
  return -1;
}

// "heart" or "heart-inside". False for anything off the list, and for the inside of something without one: then the
// question gets the normal picture rather than a model that doesn't answer it.
inline bool anatomyParseQuery(const char* query, int& index, bool& wantCut) {
  index = -1;
  wantCut = false;
  if (!query) return false;
  const size_t length = strlen(query);
  const size_t suffix = sizeof(ANATOMY_INSIDE_SUFFIX) - 1;
  char id[24];
  bool inside = false;
  if (length > suffix && length - suffix < sizeof(id) && strcmp(query + length - suffix, ANATOMY_INSIDE_SUFFIX) == 0) {
    memcpy(id, query, length - suffix);
    id[length - suffix] = 0;
    inside = true;
  } else if (length < sizeof(id)) {
    memcpy(id, query, length + 1);
  } else {
    return false;
  }
  index = anatomyFindId(id);
  if (index < 0) return false;
  if (inside && ANATOMY_KINDS[index] == ANATOMY_OUTSIDE_ONLY) {
    index = -1;
    return false;
  }
  wantCut = inside || ANATOMY_KINDS[index] == ANATOMY_CUT_ONLY;
  return true;
}

inline uint32_t anatomyCrc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

// Text the small screen can show: ends inside its field, valid UTF-8 up to U+07FF, no control characters
inline bool anatomyTextOk(const uint8_t* field, size_t size, bool mayBeEmpty) {
  const uint8_t* end = (const uint8_t*)memchr(field, 0, size);
  if (!end || (end == field && !mayBeEmpty)) return false;
  for (const uint8_t* p = field; p < end; p++) {
    if (*p < 0x20 || *p == 0x7F) return false;
    if (*p < 0x80) continue;
    if (*p < 0xC2 || *p > 0xDF || p + 1 >= end || (p[1] & 0xC0) != 0x80) return false;
    p++;
  }
  return true;
}

// Checks everything before anything is used. data must be 2-byte aligned and outlive the model.
// False for anything unexpected: the caller shows the picture instead. expectCut says which of the two views this
// must be. The flags have to match the closed list, so a cell can't show as data, nor an organ as a drawing.
inline bool anatomyParse(const uint8_t* data, size_t length, const char* expectedId, bool expectCut, AnatomyModel& out) {
  memset(&out, 0, sizeof(out));
  if (!data || !expectedId || ((uintptr_t)data & 1) != 0) return false;
  const int index = anatomyFindId(expectedId);
  if (index < 0) return false;
  if (length < ANATOMY_HEADER_SIZE + 4 || length > ANATOMY_BODY_LIMIT) return false;
  if (memcmp(data, "ZAN1", 4) != 0) return false;
  const uint8_t flags = data[4];
  const uint8_t paletteCount = data[5];
  const uint16_t vertexCount = (uint16_t)(data[6] | (data[7] << 8));
  const uint16_t triangleCount = (uint16_t)(data[8] | (data[9] << 8));
  if ((flags & ~(ANATOMY_FLAG_SCHEMATIC | ANATOMY_FLAG_CUT)) != 0) return false;
  if (((flags & ANATOMY_FLAG_CUT) != 0) != expectCut) return false;
  if (((flags & ANATOMY_FLAG_SCHEMATIC) != 0) != (ANATOMY_KINDS[index] == ANATOMY_CUT_ONLY)) return false;
  if (paletteCount < 1 || paletteCount > ANATOMY_MAX_PALETTE) return false;
  if (vertexCount < 3 || vertexCount > ANATOMY_MAX_VERTICES) return false;
  if (triangleCount < 1 || triangleCount > ANATOMY_MAX_TRIANGLES) return false;
  const uint16_t frontFrom = (uint16_t)(data[10] | (data[11] << 8));
  if (frontFrom != 0 && (!expectCut || frontFrom >= triangleCount)) return false;
  // The counts are capped above, so this can't overflow
  const size_t verticesAt = ANATOMY_HEADER_SIZE;
  const size_t trianglesAt = verticesAt + (size_t)vertexCount * 6;
  const size_t colorsAt = trianglesAt + (size_t)triangleCount * 6;
  const size_t legendAt = colorsAt + triangleCount;
  // Read before the CRC is trusted, so the length has to be there first
  if (expectCut && length < legendAt + 1 + 4) return false;
  const uint8_t legendCount = expectCut ? data[legendAt] : 0;
  if (expectCut && (legendCount < 1 || legendCount > ANATOMY_MAX_LEGEND)) return false;
  const size_t crcAt = expectCut ? legendAt + 1 + (size_t)legendCount * ANATOMY_LEGEND_SIZE : legendAt;
  if (length != crcAt + 4) return false;
  uint32_t storedCrc;
  memcpy(&storedCrc, data + crcAt, 4);
  if (storedCrc != anatomyCrc32(data, crcAt)) return false;

  if (!anatomyTextOk(data + 12, 16, false) || strcmp((const char*)data + 12, expectedId) != 0) return false;
  if (!anatomyTextOk(data + 28, 24, false) || !anatomyTextOk(data + 52, 24, false)) return false;
  if (!anatomyTextOk(data + 76, 24, false)) return false;

  const int16_t* vertices = (const int16_t*)(data + verticesAt);
  const uint16_t* triangles = (const uint16_t*)(data + trianglesAt);
  const uint8_t* colors = data + colorsAt;
  // Radius 1 is 32767; a little over is rounding, far over would run past the camera
  const int64_t limit = (int64_t)33100 * 33100;
  for (uint16_t i = 0; i < vertexCount; i++) {
    const int64_t x = vertices[i * 3], y = vertices[i * 3 + 1], z = vertices[i * 3 + 2];
    if (x * x + y * y + z * z > limit) return false;
  }
  for (uint16_t i = 0; i < triangleCount; i++) {
    const uint16_t a = triangles[i * 3], b = triangles[i * 3 + 1], c = triangles[i * 3 + 2];
    if (a >= vertexCount || b >= vertexCount || c >= vertexCount) return false;
    if (a == b || b == c || a == c) return false;
    if (colors[i] >= paletteCount) return false;
  }

  const uint8_t* legend = expectCut ? data + legendAt + 1 : nullptr;
  for (uint8_t i = 0; i < legendCount; i++) {
    const uint8_t* line = legend + (size_t)i * ANATOMY_LEGEND_SIZE;
    if (line[0] != ANATOMY_LEGEND_NOTE && line[0] >= paletteCount) return false;
    for (int language = 0; language < 2; language++) {
      const uint8_t* text = line + 1 + language * ANATOMY_LEGEND_TEXT;
      if (!anatomyTextOk(text, ANATOMY_LEGEND_TEXT, false)) return false;
      // The small font on the stick only knows ASCII
      for (int k = 0; text[k]; k++) {
        if (text[k] >= 0x80) return false;
      }
    }
  }

  out.flags = flags;
  out.frontFrom = frontFrom;
  out.legendCount = legendCount;
  out.legend = legend;
  out.paletteCount = paletteCount;
  out.vertexCount = vertexCount;
  out.triangleCount = triangleCount;
  out.id = (const char*)data + 12;
  out.labelDe = (const char*)data + 28;
  out.labelEn = (const char*)data + 52;
  out.source = (const char*)data + 76;
  memcpy(out.palette, data + 100, sizeof(out.palette));
  out.vertices = vertices;
  out.triangles = triangles;
  out.colors = colors;
  return true;
}

// Palette index of a legend line, ANATOMY_LEGEND_NOTE for a note
inline uint8_t anatomyLegendColor(const AnatomyModel& m, int line) {
  return m.legend[(size_t)line * ANATOMY_LEGEND_SIZE];
}

inline const char* anatomyLegendText(const AnatomyModel& m, int line, bool german) {
  return (const char*)m.legend + (size_t)line * ANATOMY_LEGEND_SIZE + 1 + (german ? 0 : ANATOMY_LEGEND_TEXT);
}

// What Button C does: the view asked for first, then the other one if there is one, then the text answer
struct AnatomyViews {
  bool hasOutside, hasCut, cutFirst, showingCut;
};

inline void anatomyViewsOpen(AnatomyViews& v) {
  v.showingCut = v.hasCut && (v.cutFirst || !v.hasOutside);
}

// True if the press moved to the other view; false means back to the text.
inline bool anatomyViewsPressC(AnatomyViews& v) {
  if (!v.hasOutside || !v.hasCut || v.showingCut != v.cutFirst) return false;
  v.showingCut = !v.showingCut;
  return true;
}

// The cut must face the viewer, or there's nothing to see: the back of a half organ. The hand can turn it this far.
static const float ANATOMY_CUT_LIMIT_COS = 0.5736f;  // cos 55 degrees; same as CUT_VIEW_LIMIT_DEG in the data script
static const float ANATOMY_CUT_LIMIT_RAD = 0.9599f;
static const float ANATOMY_CUT_START_X = 0.12f;
static const float ANATOMY_CUT_START_Y = 0.32f;
static const float ANATOMY_CUT_SWAY_RAD = 0.2f;  // Rocks a little by itself, so it still reads as 3D

inline void anatomyCutStart(Mat3& r) {
  mat3Identity(r);
  mat3Rotate(r, ANATOMY_CUT_START_X, ANATOMY_CUT_START_Y, 0.0f);
}

// Turns r back as little as needed so the cut faces the viewer again
inline void anatomyClampCut(Mat3& r) {
  const float nx = r.m[2], ny = r.m[5], nz = r.m[8];
  if (nz >= ANATOMY_CUT_LIMIT_COS) return;
  float ax = ny, ay = -nx;  // n x z: turns n towards the viewer
  const float length = sqrtf(ax * ax + ay * ay);
  if (length < 1e-5f) {  // Facing straight away: either way round is as short
    ax = 1.0f;
    ay = 0.0f;
  } else {
    ax /= length;
    ay /= length;
  }
  const float angle = acosf(nz < -1.0f ? -1.0f : nz) - ANATOMY_CUT_LIMIT_RAD;
  mat3Rotate(r, ax * angle, ay * angle, 0.0f);
}

// The shared view has just turned r: by the hand, or by its idle spin when the hand is still. A cut ignores the idle spin,
// which would turn it away. held is last frame's turn.
inline void anatomyHoldCut(Mat3& r, Mat3& held, bool handMoving) {
  if (!handMoving) r = held;
  anatomyClampCut(r);
  held = r;
}

// One unit normal per triangle, 3 values of -127..127, so the light costs one dot product per face
inline void anatomyComputeNormals(const AnatomyModel& m, int8_t* normals) {
  for (uint16_t i = 0; i < m.triangleCount; i++) {
    const int16_t* a = m.vertices + m.triangles[i * 3] * 3;
    const int16_t* b = m.vertices + m.triangles[i * 3 + 1] * 3;
    const int16_t* c = m.vertices + m.triangles[i * 3 + 2] * 3;
    const float ux = (float)b[0] - a[0], uy = (float)b[1] - a[1], uz = (float)b[2] - a[2];
    const float vx = (float)c[0] - a[0], vy = (float)c[1] - a[1], vz = (float)c[2] - a[2];
    const float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    const float len = sqrtf(nx * nx + ny * ny + nz * nz);
    const float s = len > 0 ? 127.0f / len : 0;
    normals[i * 3] = (int8_t)(nx * s);
    normals[i * 3 + 1] = (int8_t)(ny * s);
    normals[i * 3 + 2] = (int8_t)(nz * s);
  }
}

// Each palette colour in ANATOMY_SHADES steps from shadow to full light
inline void anatomyBuildShades(const AnatomyModel& m, uint16_t* table) {
  for (int p = 0; p < ANATOMY_MAX_PALETTE; p++) {
    const uint16_t color = p < m.paletteCount ? m.palette[p] : 0;
    const int r = color >> 11, g = (color >> 5) & 0x3F, b = color & 0x1F;
    for (int s = 0; s < ANATOMY_SHADES; s++) {
      // A third of the light is always there, so the shadow side still shows its shape
      const int k = 85 + (s * (256 - 85)) / (ANATOMY_SHADES - 1);
      table[p * ANATOMY_SHADES + s] = (uint16_t)((((r * k) >> 8) << 11) | (((g * k) >> 8) << 5) | ((b * k) >> 8));
    }
  }
}

// Projects, drops faces turned away, sorts far to near and calls draw(x0, y0, x1, y1, x2, y2, color)
// for each visible triangle, then the filled ones of a cut. Linear in triangles whatever the turn, so no frame takes
// much longer than another.
// Returns the number of triangles drawn.
template <typename DrawTriangle>
inline uint16_t anatomyRender(const AnatomyModel& m, const int8_t* normals, const uint16_t* shades, const Mat3& rotation,
                              float scale, float camera, int cx, int cy, int width, int height, AnatomyWork& w,
                              DrawTriangle draw) {
  const float unit = 1.0f / 32767.0f;
  const float* r = rotation.m;
  for (uint16_t i = 0; i < m.vertexCount; i++) {
    const float x = m.vertices[i * 3] * unit, y = m.vertices[i * 3 + 1] * unit, z = m.vertices[i * 3 + 2] * unit;
    const float px = r[0] * x + r[1] * y + r[2] * z;
    const float py = r[3] * x + r[4] * y + r[5] * z;
    const float pz = r[6] * x + r[7] * y + r[8] * z;
    const float perspective = camera / (camera - pz);
    w.sx[i] = (int16_t)(cx + px * scale * perspective);
    w.sy[i] = (int16_t)(cy - py * scale * perspective);
    w.depth[i] = (int16_t)(pz * 1024.0f);
  }

  // The light stays with the viewer, up and to the left. Turned into model space it needs one dot product per face.
  const float lx = -0.40f, ly = 0.55f, lz = 0.73f;
  const int light[3] = {(int)((r[0] * lx + r[3] * ly + r[6] * lz) * 256), (int)((r[1] * lx + r[4] * ly + r[7] * lz) * 256),
                        (int)((r[2] * lx + r[5] * ly + r[8] * lz) * 256)};

  // Screen y points down, so a face turned to the viewer has a negative cross product
  const auto facesViewer = [&](uint16_t a, uint16_t b, uint16_t c) {
    const int32_t cross = (int32_t)(w.sx[b] - w.sx[a]) * (w.sy[c] - w.sy[a]) - (int32_t)(w.sy[b] - w.sy[a]) * (w.sx[c] - w.sx[a]);
    if (cross >= 0) return false;
    const int32_t minX = anatomyMin3(w.sx[a], w.sx[b], w.sx[c]), maxX = anatomyMax3(w.sx[a], w.sx[b], w.sx[c]);
    const int32_t minY = anatomyMin3(w.sy[a], w.sy[b], w.sy[c]), maxY = anatomyMax3(w.sy[a], w.sy[b], w.sy[c]);
    return !(maxX < 0 || maxY < 0 || minX >= width || minY >= height);
  };
  const uint16_t sortedCount = m.frontFrom ? m.frontFrom : m.triangleCount;

  for (int i = 0; i < ANATOMY_DEPTH_BUCKETS; i++) w.heads[i] = ANATOMY_NO_TRIANGLE;
  for (uint16_t i = 0; i < sortedCount; i++) {
    const uint16_t a = m.triangles[i * 3], b = m.triangles[i * 3 + 1], c = m.triangles[i * 3 + 2];
    if (!facesViewer(a, b, c)) continue;
    const int32_t depth = ((int32_t)w.depth[a] + w.depth[b] + w.depth[c]) / 3;
    int bucket = (depth + 1100) * ANATOMY_DEPTH_BUCKETS / 2200;
    if (bucket < 0) bucket = 0;
    if (bucket >= ANATOMY_DEPTH_BUCKETS) bucket = ANATOMY_DEPTH_BUCKETS - 1;
    w.next[i] = w.heads[bucket];
    w.heads[bucket] = i;
  }

  uint16_t drawn = 0;
  const auto drawOne = [&](uint16_t i) {
    const uint16_t a = m.triangles[i * 3], b = m.triangles[i * 3 + 1], c = m.triangles[i * 3 + 2];
    const int dot = (normals[i * 3] * light[0] + normals[i * 3 + 1] * light[1] + normals[i * 3 + 2] * light[2]) / 127;
    int shade = dot <= 0 ? 0 : (dot * (ANATOMY_SHADES - 1)) / 256;
    if (shade >= ANATOMY_SHADES) shade = ANATOMY_SHADES - 1;
    if (i >= sortedCount) shade = ANATOMY_FILL_SHADE;  // A flat cut stays the same bright colour whichever way it's turned
    draw(w.sx[a], w.sy[a], w.sx[b], w.sy[b], w.sx[c], w.sy[c], shades[m.colors[i] * ANATOMY_SHADES + shade]);
    drawn++;
  };
  for (int bucket = 0; bucket < ANATOMY_DEPTH_BUCKETS; bucket++) {
    for (uint16_t i = w.heads[bucket]; i != ANATOMY_NO_TRIANGLE; i = w.next[i]) drawOne(i);
  }
  for (uint16_t i = sortedCount; i < m.triangleCount; i++) {
    if (facesViewer(m.triangles[i * 3], m.triangles[i * 3 + 1], m.triangles[i * 3 + 2])) drawOne(i);
  }
  return drawn;
}
