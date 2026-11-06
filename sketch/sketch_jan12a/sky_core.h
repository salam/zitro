// Pure logic of the sky view: lookup in the closed constellation list, looking around from inside
// the celestial sphere, and star appearance. Plain C++ with no Arduino includes, tested on the
// host: tests/sky/run.sh
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "model_core.h"
#include "sky_data.h"

static const int SKY_NOT_FOUND = -1;
static const int SKY_WHOLE_DOME = -2;
static const float SKY_FOCAL[3] = {120.0f, 240.0f, 480.0f};  // Pixels. 90, 53 and 28 degrees across
static const float SKY_NEAR = 0.05f;  // Lines are cut here; closer to the side of the eye they'd blow up
static const size_t SKY_FOLD_MAX = 64;

// Query to key: lower-case letters only, umlauts without dots, "the" and "constellation" dropped.
// Only letters, spaces and ' . - count as part of a name; anything else means no match at all.
inline bool skyFoldName(const char* text, char* out, size_t capacity) {
  if (!text || capacity < 2) return false;
  size_t n = 0;
  const unsigned char* p = (const unsigned char*)text;
  while (*p) {
    char a = 0, b = 0;
    if (*p >= 'a' && *p <= 'z') {
      a = (char)*p;
    } else if (*p >= 'A' && *p <= 'Z') {
      a = (char)(*p - 'A' + 'a');
    } else if (*p == ' ' || *p == '\'' || *p == '.' || *p == '-') {
      p++;
      continue;
    } else if (*p == 0xC3) {  // UTF-8 for the letters German names need
      switch (p[1]) {
        case 0xA4: case 0x84: a = 'a'; break;
        case 0xB6: case 0x96: a = 'o'; break;
        case 0xBC: case 0x9C: a = 'u'; break;
        case 0x9F: a = 's'; b = 's'; break;
        default: return false;
      }
      p++;
    } else {
      return false;
    }
    p++;
    if (n + (b ? 2 : 1) >= capacity) return false;
    out[n++] = a;
    if (b) out[n++] = b;
  }
  out[n] = 0;
  static const char SUFFIX[] = "constellation";
  const size_t suffixLen = sizeof(SUFFIX) - 1;
  if (n > suffixLen && strcmp(out + n - suffixLen, SUFFIX) == 0) {
    n -= suffixLen;
    out[n] = 0;
  }
  if (n > 3 && memcmp(out, "the", 3) == 0) {
    memmove(out, out + 3, n - 3 + 1);
    n -= 3;
  }
  return n > 0;
}

// Constellation index, SKY_WHOLE_DOME for "all", SKY_NOT_FOUND for anything off the list
inline int skyFindConstellation(const char* query) {
  char key[SKY_FOLD_MAX];
  if (!skyFoldName(query, key, sizeof(key))) return SKY_NOT_FOUND;
  if (strcmp(key, "all") == 0) return SKY_WHOLE_DOME;
  int low = 0, high = SKY_KEY_COUNT - 1;
  while (low <= high) {
    const int middle = (low + high) / 2;
    const int order = strcmp(key, SKY_KEYS[middle].key);
    if (order == 0) return SKY_KEYS[middle].constellation;
    if (order < 0) {
      high = middle - 1;
    } else {
      low = middle + 1;
    }
  }
  return SKY_NOT_FOUND;
}

// Turn that looks straight at a direction on the sphere, with celestial north up.
// Rows are the screen axes: x right, y up, z towards the viewer; what you look at ends up at z = -1.
// Seen from inside the sphere, east (growing right ascension) is then on the left, as in the real sky.
inline void skyLookAt(float x, float y, float z, Mat3& r) {
  const float length = sqrtf(x * x + y * y + z * z);
  if (length < 1e-6f) {
    mat3Identity(r);
    return;
  }
  const float fx = x / length, fy = y / length, fz = z / length;
  // North, or the x axis for a view straight at a pole
  float nx = 0, ny = 0, nz = 1;
  if (fabsf(fz) > 0.999f) {
    nx = 1;
    nz = 0;
  }
  const float d = nx * fx + ny * fy + nz * fz;
  float ux = nx - d * fx, uy = ny - d * fy, uz = nz - d * fz;
  const float ul = sqrtf(ux * ux + uy * uy + uz * uz);
  ux /= ul; uy /= ul; uz /= ul;
  // Right = forward x up
  r.m[0] = fy * uz - fz * uy;
  r.m[1] = fz * ux - fx * uz;
  r.m[2] = fx * uy - fy * ux;
  r.m[3] = ux; r.m[4] = uy; r.m[5] = uz;
  r.m[6] = -fx; r.m[7] = -fy; r.m[8] = -fz;
}

// The last row alone says whether a star is in front; for the whole table this is the cheap test.
// Returns z in units of the table, lower is further in front.
inline float skyDepth(const Mat3& r, const SkyStar& s) {
  return r.m[6] * s.x + r.m[7] * s.y + r.m[8] * s.z;
}

// Screen position of a point in view coordinates. False if it's behind the near plane.
inline bool skyProject(const float p[3], float focal, float cx, float cy, float& sx, float& sy) {
  if (p[2] > -SKY_NEAR) return false;
  const float k = focal / -p[2];
  sx = cx + p[0] * k;
  sy = cy - p[1] * k;
  return true;
}

// Cuts a line in view coordinates at the near plane. False if none of it is in front.
inline bool skyClipLine(float a[3], float b[3]) {
  const bool aFront = a[2] <= -SKY_NEAR, bFront = b[2] <= -SKY_NEAR;
  if (!aFront && !bFront) return false;
  if (aFront && bFront) return true;
  float* back = aFront ? b : a;
  const float* front = aFront ? a : b;
  const float t = (-SKY_NEAR - front[2]) / (back[2] - front[2]);
  for (int i = 0; i < 3; i++) back[i] = front[i] + (back[i] - front[i]) * t;
  back[2] = -SKY_NEAR;
  return true;
}

// Largest zoom step at which a circle of this radius fits the picture's height. The largest ones never fit.
inline uint8_t skyFitZoom(uint8_t radiusDeg, float halfHeight) {
  uint8_t zoom = 0;
  for (uint8_t i = 1; i < 3; i++) {
    if (atanf(halfHeight / SKY_FOCAL[i]) * 180.0f / (float)M_PI >= radiusDeg) zoom = i;
  }
  return zoom;
}

// Where the arrow to something outside the picture goes: on the inner edge of the rectangle, in
// the direction (dx, dy) from its centre, screen coordinates. Also gives that direction with length 1.
inline void skyEdgePoint(float dx, float dy, float cx, float cy, float halfWidth, float halfHeight, float& x, float& y,
                         float& ux, float& uy) {
  float length = sqrtf(dx * dx + dy * dy);
  if (length < 1e-6f) {  // Straight behind: any way round is as good
    dx = 1;
    dy = 0;
    length = 1;
  }
  ux = dx / length;
  uy = dy / length;
  const float kx = fabsf(ux) > 1e-6f ? halfWidth / fabsf(ux) : 1e9f;
  const float ky = fabsf(uy) > 1e-6f ? halfHeight / fabsf(uy) : 1e9f;
  const float k = kx < ky ? kx : ky;
  x = cx + ux * k;
  y = cy + uy * k;
}

// Size class on screen by magnitude: 0 is the brightest
inline uint8_t skyStarClass(uint8_t mag10) {
  if (mag10 < 25) return 0;  // Up to magnitude 0.5
  if (mag10 < 35) return 1;  // 1.5
  if (mag10 < 45) return 2;  // 2.5
  if (mag10 < 55) return 3;  // 3.5
  if (mag10 < 65) return 4;  // 4.5
  return 5;
}

// Dot radius: 0 means a single pixel
inline uint8_t skyStarRadius(uint8_t starClass) {
  return starClass == 0 ? 2 : starClass <= 2 ? 1 : 0;
}

// Star colour in RGB565. The B-V band is only a hint, well towards white; the class dims it.
inline uint16_t skyStarColor565(uint8_t starClass, uint8_t band) {
  static const uint8_t RGB[6][3] = {{165, 190, 255}, {205, 220, 255}, {255, 255, 255},
                                    {255, 245, 215}, {255, 220, 170}, {255, 190, 130}};
  static const uint8_t LEVEL[6] = {255, 255, 235, 200, 150, 110};
  if (band > 5) band = 2;
  if (starClass > 5) starClass = 5;
  const uint16_t level = LEVEL[starClass];
  const uint16_t r = RGB[band][0] * level / 255, g = RGB[band][1] * level / 255, b = RGB[band][2] * level / 255;
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
