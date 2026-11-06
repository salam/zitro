// Pure logic of the globe: coastline file, turning to face the pin, drawing the near side,
// finding the pin and the flick. Plain C++ with no Arduino includes, tested on the host: tests/globe/run.sh
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const size_t GLOBE_FILE_HEADER = 10;
static const size_t GLOBE_FILE_LIMIT = 64 * 1024;  // The most a download may be
static const uint16_t GLOBE_MAX_LINES = 4000;
static const uint16_t GLOBE_MAX_POINTS = 20000;
static const int GLOBE_MAX_LAT = 9000;   // Hundredths of a degree
static const int GLOBE_MAX_LON = 18000;
static const float GLOBE_DEG = (float)M_PI / 180.0f;
static const float GLOBE_Q14 = 16384.0f;  // Unit vectors and the rotation are 14-bit fixed point

inline uint16_t globeReadU16(const uint8_t* p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

inline int16_t globeReadI16(const uint8_t* p) {
  return (int16_t)globeReadU16(p);
}

// Checks a whole coastline file, built in or downloaded. Nothing else may read it before this says yes.
inline bool globeCheckFile(const uint8_t* data, size_t size, uint16_t& lineCount, uint16_t& pointCount) {
  lineCount = pointCount = 0;
  if (!data || size < GLOBE_FILE_HEADER) return false;
  if (memcmp(data, "ZGLB", 4) != 0 || data[4] != 1 || data[5] != 0) return false;
  const uint16_t lines = globeReadU16(data + 6);
  const uint16_t points = globeReadU16(data + 8);
  if (lines < 1 || lines > GLOBE_MAX_LINES || points < 2 || points > GLOBE_MAX_POINTS) return false;
  if (size != GLOBE_FILE_HEADER + 2 * (size_t)lines + 4 * (size_t)points) return false;
  const uint8_t* lengths = data + GLOBE_FILE_HEADER;
  uint32_t sum = 0;
  for (uint16_t i = 0; i < lines; i++) {
    const uint16_t n = globeReadU16(lengths + 2 * i);
    if (n < 2) return false;
    sum += n;
  }
  if (sum != points) return false;
  const uint8_t* p = lengths + 2 * (size_t)lines;
  for (uint16_t i = 0; i < points; i++, p += 4) {
    if (abs(globeReadI16(p)) > GLOBE_MAX_LAT || abs(globeReadI16(p + 2)) > GLOBE_MAX_LON) return false;
  }
  lineCount = lines;
  pointCount = points;
  return true;
}

// Fills lengths[lineCount] and xyz[3 * pointCount] from a file globeCheckFile has accepted
inline void globeDecode(const uint8_t* data, uint16_t lineCount, uint16_t pointCount, uint16_t* lengths, int16_t* xyz) {
  const uint8_t* in = data + GLOBE_FILE_HEADER;
  for (uint16_t i = 0; i < lineCount; i++, in += 2) lengths[i] = globeReadU16(in);
  for (uint16_t i = 0; i < pointCount; i++, in += 4, xyz += 3) {
    const float lat = globeReadI16(in) * 0.01f * GLOBE_DEG;
    const float lon = globeReadI16(in + 2) * 0.01f * GLOBE_DEG;
    const float c = cosf(lat);
    xyz[0] = (int16_t)lroundf(c * cosf(lon) * GLOBE_Q14);
    xyz[1] = (int16_t)lroundf(c * sinf(lon) * GLOBE_Q14);
    xyz[2] = (int16_t)lroundf(sinf(lat) * GLOBE_Q14);
  }
}

inline void globeUnitVector(float latDeg, float lonDeg, float out[3]) {
  const float lat = latDeg * GLOBE_DEG, lon = lonDeg * GLOBE_DEG;
  out[0] = cosf(lat) * cosf(lon);
  out[1] = cosf(lat) * sinf(lon);
  out[2] = sinf(lat);
}

// The rotation that brings this place to the middle of the screen, north up: rows are east, north, the place.
// Screen axes as in model_core.h: x right, y up, z towards the viewer. Also holds at the poles.
inline void globeFacing(float latDeg, float lonDeg, float m[9]) {
  const float lat = latDeg * GLOBE_DEG, lon = lonDeg * GLOBE_DEG;
  const float sinLat = sinf(lat), cosLat = cosf(lat), sinLon = sinf(lon), cosLon = cosf(lon);
  const float r[9] = {-sinLon,          cosLon,           0,
                      -sinLat * cosLon, -sinLat * sinLon, cosLat,
                      cosLat * cosLon,  cosLat * sinLon,  sinLat};
  memcpy(m, r, sizeof(r));
}

inline void globeRotationToQ14(const float m[9], int32_t q[9]) {
  for (int i = 0; i < 9; i++) {
    float v = isfinite(m[i]) ? m[i] : 0.0f;
    v = v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
    q[i] = (int32_t)lroundf(v * GLOBE_Q14);
  }
}

// Calls line(x0, y0, x1, y1) for every piece of coast on the near side, in screen pixels. Returns how many.
// The far side is z < 0 and looks through nothing: the view is orthographic, so that's exactly the disc's
// edge. z comes first and skips x and y for hidden points, about half of the work. A piece that stays on
// one pixel is left out and the next one joins on, so the count is limited by the screen, not by the data.
template <class LineFn>
inline uint32_t globeForEachSegment(const uint16_t* lengths, uint16_t lineCount, const int16_t* xyz, const int32_t q[9],
                                    int cx, int cy, int radius, LineFn&& line) {
  uint32_t segments = 0;
  const int16_t* p = xyz;
  auto screenX = [&](int32_t rotated) { return cx + (int)((rotated * radius) >> 14); };
  auto screenY = [&](int32_t rotated) { return cy - (int)((rotated * radius) >> 14); };
  auto rotX = [&](const int16_t* v) { return (q[0] * v[0] + q[1] * v[1] + q[2] * v[2]) >> 14; };
  auto rotY = [&](const int16_t* v) { return (q[3] * v[0] + q[4] * v[1] + q[5] * v[2]) >> 14; };
  // Where the piece from a to b crosses the edge of the disc
  auto edge = [&](const int16_t* a, int32_t za, const int16_t* b, int32_t zb, int& x, int& y) {
    const float t = (float)za / ((float)za - (float)zb);
    const int32_t ax = rotX(a), ay = rotY(a);
    x = screenX(ax + (int32_t)(t * (rotX(b) - ax)));
    y = screenY(ay + (int32_t)(t * (rotY(b) - ay)));
  };
  for (uint16_t l = 0; l < lineCount; l++) {
    const uint16_t n = lengths[l];
    int lastX = 0, lastY = 0;
    int32_t prevZ = -1;
    for (uint16_t i = 0; i < n; i++, p += 3) {
      const int32_t z = q[6] * p[0] + q[7] * p[1] + q[8] * p[2];
      if (z >= 0) {
        const int x = screenX(rotX(p)), y = screenY(rotY(p));
        if (i > 0 && prevZ < 0) {
          edge(p - 3, prevZ, p, z, lastX, lastY);
          if (x != lastX || y != lastY) { line(lastX, lastY, x, y); segments++; }
        } else if (i > 0 && (x != lastX || y != lastY)) {
          line(lastX, lastY, x, y);
          segments++;
        }
        lastX = x;
        lastY = y;
      } else if (i > 0 && prevZ >= 0) {
        int x, y;
        edge(p - 3, prevZ, p, z, x, y);
        if (x != lastX || y != lastY) { line(lastX, lastY, x, y); segments++; }
      }
      prevZ = z;
    }
  }
  return segments;
}

// Draws straight into a 16-bit sprite buffer, which is far quicker per piece than a general line call.
// Everything outside width x height is left alone, whatever the coordinates. Returns pixels written.
inline uint32_t globeRasterLine(uint16_t* buffer, int width, int height, int x0, int y0, int x1, int y1, uint16_t raw) {
  if ((x0 < 0 && x1 < 0) || (y0 < 0 && y1 < 0) || (x0 >= width && x1 >= width) || (y0 >= height && y1 >= height)) return 0;
  const int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
  const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  uint32_t pixels = 0;
  for (;;) {
    if ((unsigned)x0 < (unsigned)width && (unsigned)y0 < (unsigned)height) {
      buffer[y0 * width + x0] = raw;
      pixels++;
    }
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
  return pixels;
}

static const int GLOBE_PIN_HEIGHT = 17;   // Tip to top of the pin's head
static const int GLOBE_ARROW_INSET = 12;  // Arrow centre, from the disc's edge and from the screen's edge

struct GlobePinView {
  bool onScreen;     // True: draw the pin with its tip at x, y. False: draw the arrow
  int x, y;
  int arrow[6];      // Triangle x0, y0 (the point), x1, y1, x2, y2
};

// Where the pin is now. If it's behind the globe, or off the screen when zoomed in, the arrow shows the way:
// straight out from the middle towards it. Exactly behind, every way is equally short: the arrow points right.
inline void globeLocatePin(const float m[9], const float pin[3], int cx, int cy, int radius, int width, int height,
                           GlobePinView& out) {
  const float px = m[0] * pin[0] + m[1] * pin[1] + m[2] * pin[2];
  const float py = m[3] * pin[0] + m[4] * pin[1] + m[5] * pin[2];
  const float pz = m[6] * pin[0] + m[7] * pin[1] + m[8] * pin[2];
  out.x = cx + (int)lroundf(px * radius);
  out.y = cy - (int)lroundf(py * radius);
  out.onScreen = pz >= 0 && out.x >= 0 && out.x < width && out.y >= GLOBE_PIN_HEIGHT && out.y < height;
  float dx = px, dy = -py;
  const float length = sqrtf(dx * dx + dy * dy);
  if (length < 0.001f) {
    dx = 1;
    dy = 0;
  } else {
    dx /= length;
    dy /= length;
  }
  float reach = (float)(radius - GLOBE_ARROW_INSET);
  const float room[2] = {(float)(dx > 0 ? width - 1 - cx - GLOBE_ARROW_INSET : cx - GLOBE_ARROW_INSET),
                         (float)(dy > 0 ? height - 1 - cy - GLOBE_ARROW_INSET : cy - GLOBE_ARROW_INSET)};
  const float along[2] = {fabsf(dx), fabsf(dy)};
  for (int i = 0; i < 2; i++) {
    if (along[i] > 0.001f && room[i] / along[i] < reach) reach = room[i] / along[i];
  }
  if (reach < 0) reach = 0;
  const float ax = cx + dx * reach, ay = cy + dy * reach;
  const float point = 10, back = 6, side = 8;
  out.arrow[0] = (int)lroundf(ax + dx * point);
  out.arrow[1] = (int)lroundf(ay + dy * point);
  out.arrow[2] = (int)lroundf(ax - dx * back - dy * side);
  out.arrow[3] = (int)lroundf(ay - dy * back + dx * side);
  out.arrow[4] = (int)lroundf(ax - dx * back + dy * side);
  out.arrow[5] = (int)lroundf(ay - dy * back - dx * side);
}

// The flick: a quick turn keeps going after the hand stops, and slows down.
// The shared view turns the globe with the hand and knows nothing of this, so this only ever adds a turn
// while the hand is about still. These two mirror MODEL_TURN_GAIN and MODEL_TURN_SIGN in model_view.ino.
static const float GLOBE_TURN_GAIN = 2.0f;
static const float GLOBE_TURN_SIGN = -1.0f;
static const float GLOBE_CATCH_DPS = 30.0f;      // Faster: the hand is moving, so it holds the globe
static const float GLOBE_FLICK_DPS = 200.0f;     // A peak this fast counts as a flick
static const float GLOBE_FLICK_WINDOW_S = 0.25f; // ...if it was this recent when the hand stopped
static const float GLOBE_SPIN_MAX = 6.0f;        // rad/s; faster strobes at 25 frames per second
static const float GLOBE_SPIN_STOP = 0.2f;       // rad/s
static const float GLOBE_SPIN_TAU_S = 1.2f;      // Slows to about a third in this time

struct GlobeSpin {
  float velocity[3];  // rad/s about the screen's axes
  float peak[3];      // Fastest hand motion lately, degrees per second
  float peakSize;
  float peakAge;      // Seconds
};

inline void globeSpinReset(GlobeSpin& s) {
  memset(&s, 0, sizeof(s));
}

inline float globeLength(const float v[3]) {
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// One frame: gyro in degrees per second as in modelGyroDps. turn is what to add this frame, in radians.
inline void globeSpinStep(GlobeSpin& s, const float gyroDps[3], float dt, float turn[3]) {
  turn[0] = turn[1] = turn[2] = 0;
  const float size = globeLength(gyroDps);
  if (!isfinite(size) || !isfinite(dt) || dt < 0) {
    globeSpinReset(s);
    return;
  }
  s.peakAge += dt;
  if (size >= GLOBE_CATCH_DPS) {
    s.velocity[0] = s.velocity[1] = s.velocity[2] = 0;
    if (size >= s.peakSize || s.peakAge > GLOBE_FLICK_WINDOW_S) {
      memcpy(s.peak, gyroDps, sizeof(s.peak));
      s.peakSize = size;
      s.peakAge = 0;
    }
    return;
  }
  if (s.peakSize >= GLOBE_FLICK_DPS && s.peakAge <= GLOBE_FLICK_WINDOW_S) {
    float scale = GLOBE_TURN_SIGN * GLOBE_TURN_GAIN * GLOBE_DEG;
    const float fastest = fabsf(scale) * s.peakSize;
    if (fastest > GLOBE_SPIN_MAX) scale *= GLOBE_SPIN_MAX / fastest;
    for (int i = 0; i < 3; i++) s.velocity[i] = scale * s.peak[i];
  }
  s.peakSize = 0;
  const float decay = expf(-dt / GLOBE_SPIN_TAU_S);
  for (int i = 0; i < 3; i++) s.velocity[i] *= decay;
  if (globeLength(s.velocity) < GLOBE_SPIN_STOP) {
    s.velocity[0] = s.velocity[1] = s.velocity[2] = 0;
  }
  for (int i = 0; i < 3; i++) turn[i] = s.velocity[i] * dt;
}
