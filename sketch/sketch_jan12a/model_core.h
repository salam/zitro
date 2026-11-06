// Pure logic of the 3D view: model references, SDF parsing, rotation and depth order.
// Plain C++ with no Arduino includes, tested on the host: tests/model/run.sh
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "atom_table.h"

#define MODEL_MAX_ATOMS 200
#define MODEL_MAX_BONDS 250

// Every kind of result. A kind whose feature isn't finished is simply not available.
static const char* const MODEL_KINDS[] = {"molecule", "music",  "graph", "recipe", "globe",    "protein",
                                          "crystal",  "sky",    "physics", "solid", "timeline", "anatomy",
                                          "wolfram"};

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
