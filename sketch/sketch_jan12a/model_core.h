// Pure logic of the 3D view: model references, SDF parsing, rotation and depth order.
// Plain C++ with no Arduino includes, tested on the host: tests/model/run.sh
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "atom_table.h"

#define MODEL_MAX_ATOMS 200
#define MODEL_MAX_BONDS 250

// Every kind of result. A kind whose feature isn't finished is simply not available.
static const char* const MODEL_KINDS[] = {"molecule", "music",  "graph", "recipe", "globe",    "protein",
                                          "crystal",  "sky",    "physics", "solid", "timeline", "anatomy",
                                          "wolfram",  "clock", "iss",     "dino"};

struct ModelRef {
  char kind[12];
  char query[64];
};

struct ModelAtom {
  float x, y, z;
  uint8_t element;
};

struct ModelBond {
  uint8_t a, b, order;
};

struct ModelMolecule {
  uint16_t atomCount, bondCount;
  ModelAtom atoms[MODEL_MAX_ATOMS];
  ModelBond bonds[MODEL_MAX_BONDS];
};

struct Mat3 {
  float m[9];
};

// "<kind>:<query>" as sent by the AI. Anything unexpected gives an empty kind and false.
inline bool parseModelRef(const char* text, ModelRef& out) {
  out.kind[0] = 0;
  out.query[0] = 0;
  if (!text) return false;
  const char* colon = strchr(text, ':');
  if (!colon) return false;
  const size_t kindLen = colon - text;
  bool known = false;
  for (const char* kind : MODEL_KINDS) {
    if (strlen(kind) == kindLen && strncmp(text, kind, kindLen) == 0) known = true;
  }
  if (!known || kindLen >= sizeof(out.kind)) return false;
  const char* query = colon + 1;
  size_t len = strlen(query);
  if (len == 0 || len >= sizeof(out.query)) return false;
  for (size_t i = 0; i < len; i++) {
    if ((unsigned char)query[i] < 0x20 || query[i] == 0x7F) return false;
  }
  memcpy(out.kind, text, kindLen);
  out.kind[kindLen] = 0;
  memcpy(out.query, query, len + 1);
  return true;
}

inline uint8_t elementFromSymbol(const char* s) {
  for (int i = 1; i < ATOM_TABLE_SIZE; i++) {
    if (strcmp(ATOM_SYMBOLS[i], s) == 0) return (uint8_t)i;
  }
  return 0;
}

inline const char* elementSymbol(uint8_t element) {
  return element < ATOM_TABLE_SIZE ? ATOM_SYMBOLS[element] : "";
}

inline uint16_t elementColor565(uint8_t element) {
  return element < ATOM_TABLE_SIZE ? ATOM_COLORS[element] : 0;
}

// Reads one line; returns its length, or -1 at the end of the text
inline int sdfNextLine(const char*& p, const char* end, const char*& line) {
  if (p >= end) return -1;
  line = p;
  const char* nl = (const char*)memchr(p, '\n', end - p);
  int len = nl ? (int)(nl - p) : (int)(end - p);
  p = nl ? nl + 1 : end;
  if (len > 0 && line[len - 1] == '\r') len--;
  return len;
}

// Fixed-width integer field of an SDF line (0-based start)
inline int sdfField(const char* line, int len, int start, int width, bool& ok) {
  if (start + width > len) { ok = false; return 0; }
  char buf[8] = {0};
  memcpy(buf, line + start, width);
  char* endp;
  long v = strtol(buf, &endp, 10);
  if (endp == buf) ok = false;
  return (int)v;
}

// V2000 SDF, first record only. Centres the atoms and scales the farthest to radius 1.
inline bool parseSdf(const char* text, size_t textLen, ModelMolecule& out) {
  out.atomCount = out.bondCount = 0;
  if (!text || textLen == 0) return false;
  const char* p = text;
  const char* end = text + textLen;
  const char* line = nullptr;
  int len = 0;
  for (int i = 0; i < 4; i++) {
    len = sdfNextLine(p, end, line);
    if (len < 0) return false;
  }
  if (len < 39 || memcmp(line + 34, "V2000", 5) != 0) return false;
  bool ok = true;
  int atoms = sdfField(line, len, 0, 3, ok);
  int bonds = sdfField(line, len, 3, 3, ok);
  if (!ok || atoms < 1 || atoms > MODEL_MAX_ATOMS || bonds < 0 || bonds > MODEL_MAX_BONDS) return false;

  float sx = 0, sy = 0, sz = 0;
  for (int i = 0; i < atoms; i++) {
    len = sdfNextLine(p, end, line);
    if (len < 34) return false;
    char num[11] = {0};
    char* endp;
    ModelAtom& a = out.atoms[i];
    memcpy(num, line, 10);
    a.x = strtof(num, &endp);
    if (endp == num) return false;
    memcpy(num, line + 10, 10);
    a.y = strtof(num, &endp);
    if (endp == num) return false;
    memcpy(num, line + 20, 10);
    a.z = strtof(num, &endp);
    if (endp == num) return false;
    char sym[4] = {0};
    int n = 0;
    for (int c = 31; c < 34; c++) {
      if (line[c] != ' ') sym[n++] = line[c];
    }
    a.element = elementFromSymbol(sym);
    if (a.element == 0) return false;
    if (!isfinite(a.x) || !isfinite(a.y) || !isfinite(a.z)) return false;
    sx += a.x; sy += a.y; sz += a.z;
  }
  for (int i = 0; i < bonds; i++) {
    len = sdfNextLine(p, end, line);
    if (len < 9) return false;
    ok = true;
    int a = sdfField(line, len, 0, 3, ok);
    int b = sdfField(line, len, 3, 3, ok);
    int order = sdfField(line, len, 6, 3, ok);
    if (!ok || a < 1 || a > atoms || b < 1 || b > atoms || a == b || order < 1) return false;
    out.bonds[i].a = (uint8_t)(a - 1);
    out.bonds[i].b = (uint8_t)(b - 1);
    out.bonds[i].order = (uint8_t)order;
  }
  // The record must be complete: a cut-off download is not a model
  bool sawEnd = false;
  while ((len = sdfNextLine(p, end, line)) >= 0) {
    if (len >= 6 && strncmp(line, "M  END", 6) == 0) { sawEnd = true; break; }
  }
  if (!sawEnd) return false;

  sx /= atoms; sy /= atoms; sz /= atoms;
  float maxR2 = 0;
  for (int i = 0; i < atoms; i++) {
    ModelAtom& a = out.atoms[i];
    a.x -= sx; a.y -= sy; a.z -= sz;
    float r2 = a.x * a.x + a.y * a.y + a.z * a.z;
    if (r2 > maxR2) maxR2 = r2;
  }
  if (!isfinite(maxR2)) return false;  // Absurdly large coordinates
  if (maxR2 > 0) {
    float s = 1.0f / sqrtf(maxR2);
    for (int i = 0; i < atoms; i++) {
      out.atoms[i].x *= s; out.atoms[i].y *= s; out.atoms[i].z *= s;
    }
  }
  out.atomCount = (uint16_t)atoms;
  out.bondCount = (uint16_t)bonds;
  return true;
}

inline void mat3Identity(Mat3& r) {
  static const float id[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  memcpy(r.m, id, sizeof(id));
}

inline void rotatePoint(const Mat3& r, float x, float y, float z, float out[3]) {
  out[0] = r.m[0] * x + r.m[1] * y + r.m[2] * z;
  out[1] = r.m[3] * x + r.m[4] * y + r.m[5] * z;
  out[2] = r.m[6] * x + r.m[7] * y + r.m[8] * z;
}

// Gram-Schmidt on the first two rows; the third is their cross product
inline void mat3Orthonormalise(Mat3& r) {
  float* a = r.m;
  float* b = r.m + 3;
  float* c = r.m + 6;
  float la = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  a[0] /= la; a[1] /= la; a[2] /= la;
  float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  b[0] -= d * a[0]; b[1] -= d * a[1]; b[2] -= d * a[2];
  float lb = sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
  b[0] /= lb; b[1] /= lb; b[2] /= lb;
  c[0] = a[1] * b[2] - a[2] * b[1];
  c[1] = a[2] * b[0] - a[0] * b[2];
  c[2] = a[0] * b[1] - a[1] * b[0];
}

// Turns by rx, ry, rz radians about the screen's axes, on top of the current rotation
inline void mat3Rotate(Mat3& r, float rx, float ry, float rz) {
  float angle = sqrtf(rx * rx + ry * ry + rz * rz);
  if (angle < 1e-6f) return;
  float x = rx / angle, y = ry / angle, z = rz / angle;
  float s = sinf(angle), c = cosf(angle), t = 1 - c;
  const float q[9] = {t * x * x + c,     t * x * y - s * z, t * x * z + s * y,
                      t * x * y + s * z, t * y * y + c,     t * y * z - s * x,
                      t * x * z - s * y, t * y * z + s * x, t * z * z + c};
  float o[9];
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      o[i * 3 + j] = q[i * 3] * r.m[j] + q[i * 3 + 1] * r.m[3 + j] + q[i * 3 + 2] * r.m[6 + j];
    }
  }
  memcpy(r.m, o, sizeof(o));
  mat3Orthonormalise(r);
}

// Hand motion in the screen's axes (x right, y up, z towards you) from the sensor's. In landscape the
// sensor's x runs along the screen's short side: getLandscapeRotation() and detectOrientation() rely on it.
inline void gyroToScreen(int rotation, float gx, float gy, float gz, float out[3]) {
  const float side = rotation == 3 ? -1.0f : 1.0f;
  out[0] = -side * gy;
  out[1] = side * gx;
  out[2] = gz;
}

// Views that stay level (the globes): turned like a steering wheel, the stick turns around the picture, which
// keeps north up for the viewer and stays exactly where it is. Only the text snaps to the nearest of the four
// ways to read it.
//
// The roll is measured against the landscape rotation locked when the view opened: 0 held that way, positive
// turned clockwise as the viewer sees it, in radians. Gravity alone sets it. Lying flat, gravity can't tell
// which way is up, and the roll holds. The gyro isn't used: an offset in it (the Plus2's start-up zero) would
// turn the picture by itself.
static const float LEVEL_FLAT_G = 0.35f;          // Less gravity in the screen's plane: lying flat, hold the roll
static const float LEVEL_FULL_G = 0.6f;           // From this much on, gravity sets the roll at full pace
static const float LEVEL_SHAKE_G = 0.35f;         // Off 1 g by more than this: the hand moves, not the gravity
static const float LEVEL_FOLLOW = 0.3f;           // Share of the way to gravity's roll per frame (25 per second)
static const float LEVEL_SNAP_RAD = 0.96f;        // 55 degrees: the text turns once the stick is this far round
static const float LEVEL_QUARTER = (float)M_PI / 2;

struct LevelState {
  float roll;    // Radians, -pi to pi
  bool known;    // False until gravity has said which way is up
  int quarter;   // The sprite's own rotation that reads upright: 0 to 3
};

inline float levelWrap(float a) {
  while (a > (float)M_PI) a -= 2 * (float)M_PI;
  while (a <= -(float)M_PI) a += 2 * (float)M_PI;
  return a;
}

inline void levelReset(LevelState& s) {
  s.roll = 0;
  s.known = false;
  s.quarter = 0;
}

// The sprite rotation that reads upright at this roll, held until the stick is LEVEL_SNAP_RAD past it
inline void levelSnap(LevelState& s, bool force) {
  const float off = levelWrap(s.roll + s.quarter * LEVEL_QUARTER);
  if (!force && fabsf(off) <= LEVEL_SNAP_RAD) return;
  s.quarter = (((int)lroundf(-s.roll / LEVEL_QUARTER)) % 4 + 4) % 4;
}

// One frame. up: what the accelerometer reads in the screen's axes (x right, y up, z towards the viewer), in g;
// at rest that's straight up.
inline void levelStep(LevelState& s, const float up[3]) {
  if (!isfinite(up[0]) || !isfinite(up[1]) || !isfinite(up[2])) return;
  const float plane = sqrtf(up[0] * up[0] + up[1] * up[1]);
  const float total = sqrtf(plane * plane + up[2] * up[2]);
  if (plane >= LEVEL_FLAT_G && fabsf(total - 1.0f) <= LEVEL_SHAKE_G) {
    const float target = atan2f(-up[0], up[1]);
    if (!s.known) {
      s.roll = target;
      s.known = true;
      levelSnap(s, true);
      return;
    }
    float weight = (plane - LEVEL_FLAT_G) / (LEVEL_FULL_G - LEVEL_FLAT_G);
    if (weight > 1) weight = 1;
    s.roll = levelWrap(s.roll + LEVEL_FOLLOW * weight * levelWrap(target - s.roll));
  }
  levelSnap(s, false);
}

// The picture's turn: the globe's own (upright) rotation, turned by the roll so north stays up for the viewer
inline void levelMatrix(const float m[9], float roll, float out[9]) {
  const float c = cosf(roll), s = sinf(roll);
  for (int j = 0; j < 3; j++) {
    out[j] = c * m[j] - s * m[3 + j];
    out[3 + j] = s * m[j] + c * m[3 + j];
    out[6 + j] = m[6 + j];
  }
}

// Hand motion as the upright picture sees it. The in-plane part is the roll, which levelMatrix already shows,
// so it doesn't also turn the globe.
inline void levelGyro(const float in[3], float roll, float out[3]) {
  const float c = cosf(roll), s = sinf(roll);
  const float x = c * in[0] + s * in[1], y = -s * in[0] + c * in[1];
  out[0] = x;
  out[1] = y;
  out[2] = 0;
}

// A pixel of the sprite's buffer (bw x bh) as the sprite's rotation q addresses it: LovyanGFX's Panel_Sprite
// maps reading coordinates to the buffer like this (drawPixelPreclipped), and this is its inverse.
inline void levelToReading(int q, float bx, float by, int bw, int bh, float& rx, float& ry) {
  switch (q & 3) {
    case 1: rx = by;          ry = bw - 1 - bx; break;
    case 2: rx = bw - 1 - bx; ry = bh - 1 - by; break;
    case 3: rx = bh - 1 - by; ry = bx;          break;
    default: rx = bx;         ry = by;          break;
  }
}

// Sorts indices far first (lowest z). Insertion sort: nearly sorted from frame to frame.
inline void sortByDepth(const float* z, uint8_t* order, uint16_t n) {
  for (uint16_t i = 1; i < n; i++) {
    uint8_t v = order[i];
    int j = i - 1;
    while (j >= 0 && z[order[j]] > z[v]) {
      order[j + 1] = order[j];
      j--;
    }
    order[j + 1] = v;
  }
}

// True if the text has only ASCII: the model views' small footer font knows nothing else
inline bool eduIsAscii(const char* s) {
  for (; *s; s++) {
    if ((uint8_t)*s >= 0x80) return false;
  }
  return true;
}

// The footer line under a model view: "label - source", the label shortened with "..." until it fits in room
// (never inside a UTF-8 character). When not even one letter of the label fits, the source alone, so a narrow
// (turned) screen doesn't lose the credit off its edge. width(text) measures in the label's font. Returns false
// when the label was left out: then the source may get the small font.
template <typename Width>
inline bool eduFooterFit(const char* label, const char* source, int room, Width width, char* out, size_t size) {
  const char* dash = *source ? " - " : "";
  snprintf(out, size, "%s%s%s", label, dash, source);
  if (width(out) <= room) return true;
  size_t keep = strlen(label);
  while (keep > 0) {
    keep--;
    while (keep > 0 && ((uint8_t)label[keep] & 0xC0) == 0x80) keep--;
    if (keep == 0) break;
    snprintf(out, size, "%.*s...%s%s", (int)keep, label, dash, source);
    if (width(out) <= room) return true;
  }
  snprintf(out, size, "%s", source);
  return false;
}
