// Draws the physics scenes. Written against any canvas type, so tests/physics draws the same
// pictures on the host. Everything that moves comes from physics_core.h.
#pragma once

#include "physics_atom_table.h"
#include "physics_core.h"

static const uint16_t PHYSICS_BLACK = 0x0000, PHYSICS_WHITE = 0xFFFF, PHYSICS_GREY = 0x8410, PHYSICS_DARK = 0x3186,
                      PHYSICS_LIGHT = 0xC618, PHYSICS_CITRUS = 0xF5A0, PHYSICS_RED = 0xF9E7, PHYSICS_BLUE = 0x2D7F,
                      PHYSICS_GREEN = 0x4F2A, PHYSICS_WATER = 0x0B38, PHYSICS_BROWN = 0x8A62, PHYSICS_GROUND = 0x3364,
                      PHYSICS_ORANGE = 0xFC40, PHYSICS_ICE = 0xAEFF;

static const int PHYSICS_W = 240;
static const int PHYSICS_H = 117;  // Above the bottom line

struct PhysicsState {
  uint8_t sceneIndex, param;
  PhysicsSceneId scene;
  bool german;
  const GFXfont* big;
  const GFXfont* normal;
  float t;        // Seconds since this run started
  float a, b, c;  // The moving quantities of the scene
  bool clockKnown;   // Sky scenes: the device knows today's date...
  double startDay;   // ...which is this Julian day
  PhysicsMatter matter;
  // Magnets: field lines, worked out once for each setting
  PhysicsPole poles[4];
  uint8_t poleCount, lineCount;
  uint8_t lineLength[PHYSICS_MAGNET_LINES];
  int16_t lineX[PHYSICS_MAGNET_LINES][PHYSICS_MAGNET_POINTS];
  int16_t lineY[PHYSICS_MAGNET_LINES][PHYSICS_MAGNET_POINTS];
};

template <class C>
void physicsText(C& c, const GFXfont* font, const char* text, int x, int y, uint16_t color, textdatum_t datum) {
  c.setFreeFont(font);
  c.setTextDatum(datum);
  c.setTextColor(color);
  c.drawString(text, x, y);
}

template <class C>
void physicsThickLine(C& c, int x0, int y0, int x1, int y1, uint16_t color) {
  c.drawLine(x0, y0, x1, y1, color);
  if (abs(x1 - x0) > abs(y1 - y0)) {
    c.drawLine(x0, y0 + 1, x1, y1 + 1, color);
    c.drawLine(x0, y0 - 1, x1, y1 - 1, color);
  } else {
    c.drawLine(x0 + 1, y0, x1 + 1, y1, color);
    c.drawLine(x0 - 1, y0, x1 - 1, y1, color);
  }
}

// Number with a unit, with the decimal mark of the device language
inline void physicsValue(char* out, size_t size, float value, int decimals, const char* unit, bool german) {
  char number[16];
  physicsFormatNumber(number, sizeof(number), value, decimals, german);
  snprintf(out, size, "%s %s", number, unit);
}

inline const char* physicsPick(const PhysicsState& s, const char* de, const char* en) { return s.german ? de : en; }

// ---- Falling ----
template <class C>
void physicsDrawFalling(C& c, PhysicsState& s, float dt) {
  const PhysicsWorld& world = PHYSICS_WORLDS[s.param];
  const float height = 5.0f, top = 3.0f, floorY = 106.0f;
  const float fall = physicsFallTime(world.g, height);
  s.t += dt;
  if (s.t > fall + 1.5f) s.t = 0;
  const float t = s.t < fall ? s.t : fall;
  const float perMetre = (floorY - top - 18.0f) / height;

  c.fillRect(0, (int)floorY, PHYSICS_W, PHYSICS_H - (int)floorY, PHYSICS_GROUND);
  c.drawFastVLine(10, (int)top, (int)(floorY - top), PHYSICS_GREY);
  char text[24];
  physicsValue(text, sizeof(text), height, 0, "m", s.german);
  physicsText(c, s.normal, text, 14, 60, PHYSICS_LIGHT, textdatum_t::baseline_left);

  // Where the ball was every fifth of a second: the gaps grow, it speeds up
  for (float k = 0.2f; k < t; k += 0.2f) {
    const int y = (int)(top + 9 + physicsFallDistance(world.g, k) * perMetre);
    c.drawCircle(68, y, 9, PHYSICS_DARK);
  }
  const float drop = physicsFallDistance(world.g, t) * perMetre;
  c.fillCircle(68, (int)(top + 9 + drop), 9, PHYSICS_RED);
  // Heavy and light fall alike: mass isn't in the equation
  c.fillCircle(100, (int)(top + 14 + drop), 4, PHYSICS_ICE);

  physicsText(c, s.big, s.german ? world.de : world.en, 126, 24, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsFormatNumber(text, sizeof(text), world.g, 2, s.german);
  char line[24];
  snprintf(line, sizeof(line), "%s m/s\xC2\xB2", text);
  physicsText(c, s.normal, line, 126, 44, PHYSICS_LIGHT, textdatum_t::baseline_left);
  physicsValue(text, sizeof(text), t, 1, "s", s.german);
  physicsText(c, s.big, text, 126, 74, PHYSICS_CITRUS, textdatum_t::baseline_left);
  physicsValue(text, sizeof(text), physicsFallSpeed(world.g, t), 1, "m/s", s.german);
  physicsText(c, s.normal, text, 126, 94, PHYSICS_CITRUS, textdatum_t::baseline_left);
}

// ---- Friction ----
template <class C>
void physicsDrawFriction(C& c, PhysicsState& s, float dt) {
  static const uint16_t colors[3] = {PHYSICS_ICE, PHYSICS_BROWN, PHYSICS_LIGHT};
  const float speed = PHYSICS_PUSH_SPEEDS[s.param];
  const float g = PHYSICS_G_EARTH, perMetre = 23.0f;
  const int startX = 64;
  s.t += dt;
  if (s.t > physicsStopTime(speed, PHYSICS_SURFACES[0].mu, g) + 2.0f) s.t = 0;

  char text[24];
  physicsValue(text, sizeof(text), speed, 1, "m/s", s.german);
  physicsText(c, s.normal, text, PHYSICS_W - 2, 15, PHYSICS_CITRUS, textdatum_t::baseline_right);
  for (int i = 0; i < 3; i++) {
    const PhysicsSurface& surface = PHYSICS_SURFACES[i];
    const int y = 38 + i * 34;
    c.fillRect(0, y, PHYSICS_W, 4, colors[i]);
    physicsText(c, s.normal, s.german ? surface.de : surface.en, 2, y - 3, colors[i], textdatum_t::baseline_left);
    const float moved = physicsSlideDistance(speed, surface.mu, g, s.t);
    const int x = startX + (int)(moved * perMetre);
    c.fillRect(x, y - 13, 15, 13, PHYSICS_WHITE);
    c.drawRect(x, y - 13, 15, 13, PHYSICS_DARK);
    if (s.t >= physicsStopTime(speed, surface.mu, g)) {
      physicsValue(text, sizeof(text), moved, moved < 1.0f ? 2 : 1, "m", s.german);
      const bool roomRight = x + 15 + 4 + c.textWidth(text) < PHYSICS_W;
      physicsText(c, s.normal, text, roomRight ? x + 19 : x - 4, y - 3, PHYSICS_WHITE,
                  roomRight ? textdatum_t::baseline_left : textdatum_t::baseline_right);
    }
  }
}

// ---- Lever ----
template <class C>
void physicsDrawLeverWeight(C& c, const PhysicsState& s, float mass, float marks, int side) {
  const int pivotX = PHYSICS_W / 2, pivotY = 84, perMark = 22;
  const float along = side * marks * perMark;
  const int x = pivotX + (int)(along * cosf(s.a));
  const int y = pivotY + (int)(along * sinf(s.a));
  const int height = mass >= 4 ? 30 : (mass >= 2 ? 22 : 16);
  c.fillRect(x - 17, y - height - 1, 34, height, side < 0 ? PHYSICS_BLUE : PHYSICS_RED);
  char text[12];
  snprintf(text, sizeof(text), "%d kg", (int)mass);
  physicsText(c, s.normal, text, x, y - height / 2 + 4, PHYSICS_WHITE, textdatum_t::baseline_center);
}

template <class C>
void physicsDrawLever(C& c, PhysicsState& s, float dt) {
  const PhysicsLeverLoad& load = PHYSICS_LEVER_LOADS[s.param];
  const float step = 0.005f;
  for (s.t += dt; s.t >= step; s.t -= step) {
    physicsLeverStep(s.a, s.b, PHYSICS_LEVER_LEFT_MASS, PHYSICS_LEVER_LEFT_MARKS, load.mass, load.marks,
                     PHYSICS_G_EARTH, step);
  }
  const int pivotX = PHYSICS_W / 2, pivotY = 84, half = 88;
  c.fillRect(0, 108, PHYSICS_W, PHYSICS_H - 108, PHYSICS_GROUND);
  c.fillTriangle(pivotX, pivotY, pivotX - 14, 108, pivotX + 14, 108, PHYSICS_LIGHT);
  const int dx = (int)(half * cosf(s.a)), dy = (int)(half * sinf(s.a));
  physicsThickLine(c, pivotX - dx, pivotY - dy, pivotX + dx, pivotY + dy, PHYSICS_CITRUS);
  for (int mark = -4; mark <= 4; mark++) {
    const float along = mark * 22.0f;
    c.fillCircle(pivotX + (int)(along * cosf(s.a)), pivotY + (int)(along * sinf(s.a)), 1, PHYSICS_BLACK);
  }
  physicsDrawLeverWeight(c, s, PHYSICS_LEVER_LEFT_MASS, PHYSICS_LEVER_LEFT_MARKS, -1);
  physicsDrawLeverWeight(c, s, load.mass, load.marks, 1);

  const int side = physicsLeverSide(PHYSICS_LEVER_LEFT_MASS, PHYSICS_LEVER_LEFT_MARKS, load.mass, load.marks);
  char text[32];
  snprintf(text, sizeof(text), "%d x %d  %s  %d x %d", (int)PHYSICS_LEVER_LEFT_MASS, (int)PHYSICS_LEVER_LEFT_MARKS,
           side == 0 ? "=" : (side < 0 ? ">" : "<"), (int)load.mass, (int)load.marks);
  physicsText(c, s.big, text, pivotX, 24, side == 0 ? PHYSICS_GREEN : PHYSICS_WHITE, textdatum_t::baseline_center);
}

// ---- Pulley ----
template <class C>
void physicsDrawPulley(C& c, PhysicsState& s, float dt) {
  const int ropes = PHYSICS_PULLEY_ROPES[s.param];
  const float weight = 100.0f, maxLift = 15.0f;  // Newton, centimetres; one pixel is a centimetre
  s.t += dt;
  const float lift = maxLift * (1.0f - cosf(2.0f * PHYSICS_PI * s.t / 6.0f)) / 2.0f;
  const float force = physicsPulleyForce(weight, ropes);
  const int r = 10, fixedY = 16;
  const int loadTop = 72 - (int)lift;
  const int movingY = loadTop - r - 2;
  const int handY = 34 + (int)physicsPulleyRope(lift, ropes);

  c.fillRect(0, 0, 128, 4, PHYSICS_GREY);
  c.fillRect(0, 108, 128, PHYSICS_H - 108, PHYSICS_GROUND);

  int boxLeft, boxWidth, handX;
  if (ropes == 1) {
    // One fixed wheel: only changes the direction of the pull
    const int wheelX = 72;
    boxLeft = 42; boxWidth = 36; handX = wheelX + 12;
    c.drawFastVLine(wheelX - 12, fixedY, loadTop - fixedY, PHYSICS_CITRUS);
    c.drawFastVLine(wheelX - 11, fixedY, loadTop - fixedY, PHYSICS_CITRUS);
    c.drawFastVLine(handX, fixedY, handY - fixedY, PHYSICS_WHITE);
    c.drawFastVLine(wheelX, 4, fixedY - 4, PHYSICS_GREY);
    c.fillCircle(wheelX, fixedY, 12, PHYSICS_LIGHT);
    c.fillCircle(wheelX, fixedY, 3, PHYSICS_DARK);
  } else {
    // Ropes hang side by side, each carries the same share of the load
    const int firstX = ropes == 2 ? 50 : 40;
    boxLeft = firstX - 4; boxWidth = (ropes - 1) * 2 * r + 8;
    handX = firstX + ropes * 2 * r;
    for (int i = 0; i < ropes; i++) {
      const int x = firstX + i * 2 * r;
      const int topY = i == 0 ? 4 : fixedY;
      c.drawFastVLine(x, topY, movingY - topY, PHYSICS_CITRUS);
      c.drawFastVLine(x + 1, topY, movingY - topY, PHYSICS_CITRUS);
    }
    c.drawFastVLine(handX, fixedY, handY - fixedY, PHYSICS_WHITE);
    for (int i = 0; i < ropes / 2; i++) {
      const int movingX = firstX + r + i * 4 * r, fixedX = movingX + 2 * r;
      c.drawFastVLine(movingX, movingY, loadTop - movingY, PHYSICS_GREY);
      c.fillCircle(movingX, movingY, r, PHYSICS_LIGHT);
      c.fillCircle(movingX, movingY, 3, PHYSICS_DARK);
      c.drawFastVLine(fixedX, 4, fixedY - 4, PHYSICS_GREY);
      c.fillCircle(fixedX, fixedY, r, PHYSICS_LIGHT);
      c.fillCircle(fixedX, fixedY, 3, PHYSICS_DARK);
    }
  }
  c.fillRect(boxLeft, loadTop, boxWidth, 24, PHYSICS_BLUE);
  // The hand, and how hard it pulls
  const int arrow = (int)(force * 0.3f);
  c.fillRect(handX - 1, handY, 3, arrow, PHYSICS_RED);
  c.fillTriangle(handX - 5, handY + arrow, handX + 5, handY + arrow, handX, handY + arrow + 6, PHYSICS_RED);
  c.fillCircle(handX, handY, 4, PHYSICS_WHITE);

  char text[24];
  const int textX = 134;
  physicsText(c, s.normal, physicsPick(s, "Last", "Load"), textX, 26, PHYSICS_BLUE, textdatum_t::baseline_left);
  physicsValue(text, sizeof(text), weight, 0, "N", s.german);
  physicsText(c, s.normal, text, PHYSICS_W - 2, 26, PHYSICS_WHITE, textdatum_t::baseline_right);
  physicsText(c, s.normal, physicsPick(s, "Kraft", "Pull"), textX, 58, PHYSICS_RED, textdatum_t::baseline_left);
  physicsValue(text, sizeof(text), force, 0, "N", s.german);
  physicsText(c, s.normal, text, PHYSICS_W - 2, 58, PHYSICS_WHITE, textdatum_t::baseline_right);
  physicsText(c, s.normal, physicsPick(s, "Seil", "Rope"), textX, 90, PHYSICS_LIGHT, textdatum_t::baseline_left);
  physicsValue(text, sizeof(text), physicsPulleyRope(maxLift, ropes), 0, "cm", s.german);
  physicsText(c, s.normal, text, PHYSICS_W - 2, 90, PHYSICS_WHITE, textdatum_t::baseline_right);
}

// ---- Float or sink ----
template <class C>
void physicsDrawBuoyancy(C& c, PhysicsState& s, float dt) {
  static const uint16_t colors[5] = {PHYSICS_ORANGE, PHYSICS_BROWN, PHYSICS_ICE, PHYSICS_GREY, PHYSICS_RED};
  const PhysicsMaterial& material = PHYSICS_MATERIALS[s.param];
  const int left = 6, right = 100, surface = 34, bottom = 110;
  const float perMetre = 200.0f, tank = (bottom - surface) / perMetre;
  const int side = (int)(PHYSICS_CUBE_SIDE * perMetre), cubeX = (left + right) / 2 - side / 2;
  s.c += dt;
  if (s.c > 7.0f) {
    s.c = 0;
    s.a = -0.04f;
    s.b = 0;
  }
  const float step = 0.002f;
  for (s.t += dt; s.t >= step; s.t -= step) physicsBuoyancyStep(s.a, s.b, material.density, tank, PHYSICS_G_EARTH, step);

  c.fillRect(left, surface, right - left, bottom - surface, PHYSICS_WATER);
  const int cubeBottom = surface + (int)(s.a * perMetre);
  c.fillRect(cubeX, cubeBottom - side, side, side, colors[s.param]);
  // The part under water looks darker
  const int underHeight = cubeBottom - surface;
  if (underHeight > 0) {
    const int clipped = underHeight > side ? side : underHeight;
    const uint16_t color = colors[s.param];
    const uint16_t dim = (uint16_t)((color >> 1) & 0x7BEF);
    c.fillRect(cubeX, cubeBottom - clipped, side, clipped, dim);
  }
  c.drawFastHLine(left, surface, right - left, PHYSICS_ICE);
  c.drawFastVLine(left - 1, surface - 12, bottom - surface + 13, PHYSICS_LIGHT);
  c.drawFastVLine(right, surface - 12, bottom - surface + 13, PHYSICS_LIGHT);
  c.drawFastHLine(left - 1, bottom, right - left + 2, PHYSICS_LIGHT);

  char number[16], text[28];
  const int textX = 108;
  physicsText(c, s.big, s.german ? material.de : material.en, textX, 24, colors[s.param], textdatum_t::baseline_left);
  physicsFormatNumber(number, sizeof(number), material.density, 2, s.german);
  snprintf(text, sizeof(text), "%s g/cm\xC2\xB3", number);
  physicsText(c, s.normal, text, textX, 44, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsFormatNumber(number, sizeof(number), PHYSICS_WATER_DENSITY, 2, s.german);
  snprintf(text, sizeof(text), "%s %s", physicsPick(s, "Wasser", "Water"), number);
  physicsText(c, s.normal, text, textX, 64, PHYSICS_ICE, textdatum_t::baseline_left);
  const bool floats = physicsFloats(material.density, PHYSICS_WATER_DENSITY);
  const char* verdict = floats ? physicsPick(s, "schwimmt", "floats") : physicsPick(s, "sinkt", "sinks");
  physicsText(c, s.big, verdict, textX, 96, floats ? PHYSICS_GREEN : PHYSICS_RED, textdatum_t::baseline_left);
}

// ---- Pendulum ----
template <class C>
void physicsDrawPendulum(C& c, PhysicsState& s, float dt) {
  const float length = PHYSICS_PENDULUM_LENGTHS[s.param];
  const int pivotX = 76, pivotY = 4;
  const float perMetre = 44.0f;
  const float period = physicsPendulumPeriod(length, PHYSICS_G_EARTH);
  const float step = 0.002f;
  // s.c: seconds since the bob last turned back at the left end, for the timing bar
  for (s.t += dt; s.t >= step; s.t -= step) {
    const float before = s.b;
    physicsPendulumStep(s.a, s.b, length, PHYSICS_G_EARTH, step);
    s.c += step;
    if (before < 0 && s.b >= 0) s.c = 0;
  }
  c.fillRect(pivotX - 30, 0, 60, 4, PHYSICS_GREY);
  const float radius = length * perMetre;
  // The swing's path
  for (int d = -25; d <= 25; d += 5) {
    const float a = d * PHYSICS_DEG;
    c.drawPixel(pivotX + (int)(radius * sinf(a)), pivotY + (int)(radius * cosf(a)), PHYSICS_GREY);
  }
  const int x = pivotX + (int)(radius * sinf(s.a));
  const int y = pivotY + (int)(radius * cosf(s.a));
  physicsThickLine(c, pivotX, pivotY, x, y, PHYSICS_LIGHT);
  c.fillCircle(x, y, 8, PHYSICS_CITRUS);

  char text[24];
  const int textX = 134;
  physicsValue(text, sizeof(text), length, length == 1.0f ? 0 : 2, "m", s.german);
  physicsText(c, s.big, text, textX, 26, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsText(c, s.normal, physicsPick(s, "Hin und her", "Full swing"), textX, 56, PHYSICS_LIGHT,
              textdatum_t::baseline_left);
  physicsValue(text, sizeof(text), period, 1, "s", s.german);
  physicsText(c, s.big, text, textX, 82, PHYSICS_CITRUS, textdatum_t::baseline_left);
  const int barWidth = 88;
  c.drawRect(textX, 92, barWidth, 10, PHYSICS_GREY);
  const float share = physicsClamp(s.c / period, 0.0f, 1.0f);
  c.fillRect(textX + 1, 93, (int)((barWidth - 2) * share), 8, PHYSICS_CITRUS);
}

// ---- Energy on a ramp ----
template <class C>
void physicsDrawEnergyBar(C& c, const PhysicsState& s, const char* word, float share, int y, uint16_t color) {
  const int x = 146, width = 90;
  physicsText(c, s.normal, word, x, y, color, textdatum_t::baseline_left);
  c.drawRect(x, y + 4, width, 11, PHYSICS_DARK);
  c.fillRect(x + 1, y + 5, (int)((width - 2) * physicsClamp(share, 0.0f, 1.0f)), 9, color);
}

template <class C>
void physicsDrawRamp(C& c, PhysicsState& s, float dt) {
  const float shape = PHYSICS_RAMP_SHAPE, g = PHYSICS_G_EARTH;
  const float loss = s.param == 1 ? PHYSICS_RAMP_LOSS : 0.0f;
  const float start = physicsRampHeightEnergy(PHYSICS_RAMP_START, shape, g);
  const float step = 0.002f;
  for (s.t += dt; s.t >= step; s.t -= step) physicsRampStep(s.a, s.b, shape, g, loss, step);

  const int middleX = 70, bottomY = 108;
  const float perMetre = 62.0f;
  int lastX = 0, lastY = 0;
  for (int i = 0; i <= 44; i++) {
    const float x = -1.1f + i * 0.05f;
    const int px = middleX + (int)(x * perMetre), py = bottomY - (int)(shape * x * x * perMetre);
    if (i > 0) physicsThickLine(c, lastX, lastY, px, py, PHYSICS_LIGHT);
    lastX = px;
    lastY = py;
  }
  const int ballX = middleX + (int)(s.a * perMetre);
  const int ballY = bottomY - (int)(shape * s.a * s.a * perMetre) - 8;
  c.fillCircle(ballX, ballY, 7, PHYSICS_RED);

  const float height = physicsRampHeightEnergy(s.a, shape, g);
  const float motion = physicsRampMotionEnergy(s.a, s.b, shape);
  const float heat = start - height - motion;
  // A ball that has nearly stopped starts again from the top
  if (s.param == 1 && height + motion < 0.02f * start) {
    s.c += dt;
    if (s.c > 2.0f) {
      s.a = PHYSICS_RAMP_START;
      s.b = 0;
      s.c = 0;
    }
  }
  physicsText(c, s.normal, s.param == 1 ? physicsPick(s, "mit Reibung", "with friction") : physicsPick(s, "ohne Reibung", "no friction"),
              2, 15, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsDrawEnergyBar(c, s, physicsPick(s, "H\xC3\xB6he", "Height"), height / start, 18, PHYSICS_BLUE);
  physicsDrawEnergyBar(c, s, physicsPick(s, "Tempo", "Speed"), motion / start, 52, PHYSICS_RED);
  physicsDrawEnergyBar(c, s, physicsPick(s, "W\xC3\xA4rme", "Heat"), heat / start, 86, PHYSICS_ORANGE);
}

// ---- Magnets ----
inline void physicsMagnetBuild(PhysicsState& s) {
  PhysicsPole* p = s.poles;
  if (s.param == 0) {
    s.poleCount = 2;
    p[0] = {96, 54, 1};
    p[1] = {144, 54, -1};
  } else {
    s.poleCount = 4;
    const float facing = s.param == 1 ? -1.0f : 1.0f;  // The left magnet's right end: south attracts, north repels
    p[0] = {30, 54, -facing};
    p[1] = {90, 54, facing};
    p[2] = {150, 54, 1};
    p[3] = {210, 54, -1};
  }
  int norths = 0;
  for (int i = 0; i < s.poleCount; i++) {
    if (p[i].strength > 0) norths++;
  }
  const int seeds = PHYSICS_MAGNET_LINES / norths;
  s.lineCount = 0;
  for (int i = 0; i < s.poleCount; i++) {
    if (p[i].strength <= 0) continue;
    for (int k = 0; k < seeds; k++) {
      const float a = 2.0f * PHYSICS_PI * (k + 0.5f) / seeds;
      const int n = physicsTraceFieldLine(p, s.poleCount, p[i].x + 3 * cosf(a), p[i].y + 3 * sinf(a), 4.0f, -8, -8,
                                          PHYSICS_W + 8, PHYSICS_H + 8, s.lineX[s.lineCount], s.lineY[s.lineCount],
                                          PHYSICS_MAGNET_POINTS);
      s.lineLength[s.lineCount++] = (uint8_t)n;
    }
  }
}

template <class C>
void physicsDrawBar(C& c, const PhysicsState& s, float northX, float southX) {
  const int y = 54, half = 9, reach = 6;
  const bool northLeft = northX < southX;
  const int left = (int)(northLeft ? northX : southX) - reach, right = (int)(northLeft ? southX : northX) + reach;
  const int middle = (left + right) / 2;
  c.fillRect(left, y - half, middle - left, 2 * half, northLeft ? PHYSICS_RED : PHYSICS_BLUE);
  c.fillRect(middle, y - half, right - middle, 2 * half, northLeft ? PHYSICS_BLUE : PHYSICS_RED);
  physicsText(c, s.normal, northLeft ? "N" : "S", (left + middle) / 2, y + 6, PHYSICS_WHITE, textdatum_t::baseline_center);
  physicsText(c, s.normal, northLeft ? "S" : "N", (middle + right) / 2, y + 6, PHYSICS_WHITE, textdatum_t::baseline_center);
}

template <class C>
void physicsDrawMagnet(C& c, PhysicsState& s, float dt) {
  s.t += dt;
  for (int i = 0; i < s.lineCount; i++) {
    const int n = s.lineLength[i];
    for (int k = 1; k < n; k++) c.drawLine(s.lineX[i][k - 1], s.lineY[i][k - 1], s.lineX[i][k], s.lineY[i][k], PHYSICS_GREY);
    // Dots run the way the field points, from north to south
    if (n > 8) {
      const int at = ((int)(s.t * 10.0f) + i * 7) % n;
      c.fillRect(s.lineX[i][at] - 1, s.lineY[i][at] - 1, 3, 3, PHYSICS_CITRUS);
    }
  }
  if (s.poleCount == 2) {
    physicsDrawBar(c, s, s.poles[0].x, s.poles[1].x);
  } else {
    physicsDrawBar(c, s, s.poles[0].strength > 0 ? s.poles[0].x : s.poles[1].x, s.poles[0].strength > 0 ? s.poles[1].x : s.poles[0].x);
    physicsDrawBar(c, s, s.poles[2].x, s.poles[3].x);
  }
  // A compass travels around and turns the way the field points where it is
  const float way = s.t * 0.35f;
  const int cx = 120 + (int)(100.0f * cosf(way)), cy = 54 + (int)(40.0f * sinf(way));
  float bx, by;
  physicsPoleField(s.poles, s.poleCount, (float)cx, (float)cy, bx, by);
  const float length = sqrtf(bx * bx + by * by);
  if (length > 0) {
    bx /= length;
    by /= length;
  }
  c.fillCircle(cx, cy, 11, PHYSICS_BLACK);
  c.drawCircle(cx, cy, 11, PHYSICS_LIGHT);
  physicsThickLine(c, cx, cy, cx + (int)(9 * bx), cy + (int)(9 * by), PHYSICS_RED);
  physicsThickLine(c, cx, cy, cx - (int)(9 * bx), cy - (int)(9 * by), PHYSICS_WHITE);

  if (s.param > 0) {
    const char* word = s.param == 1 ? physicsPick(s, "ziehen sich an", "attract") : physicsPick(s, "stossen sich ab", "repel");
    c.fillRect(0, 99, PHYSICS_W, PHYSICS_H - 99, PHYSICS_BLACK);
    physicsText(c, s.normal, word, PHYSICS_W / 2, 112, PHYSICS_WHITE, textdatum_t::baseline_center);
  }
}

// ---- Shadow: a lamp moving around an object, as in a classroom ----
template <class C>
void physicsDrawShadow(C& c, PhysicsState& s, float dt) {
  const int floorY = 92, objectX = PHYSICS_W / 2;
  const float reach = 76.0f;
  const float height = s.param == 0 ? 16.0f : 36.0f;
  s.t += dt;
  // Back and forth over the object, from low on one side to low on the other
  const float angle = (90.0f + 72.0f * cosf(2.0f * PHYSICS_PI * s.t / 12.0f)) * PHYSICS_DEG;
  const float lampX = objectX - reach * cosf(angle) * 1.4f, lampHeight = reach * sinf(angle);
  const int lx = (int)lampX, ly = floorY - (int)lampHeight;

  c.fillRect(0, floorY, PHYSICS_W, 7, PHYSICS_LIGHT);
  float endX = 0;
  const bool lands = physicsShadowEnd(lampX, lampHeight, (float)objectX, height, endX);
  // A lamp no higher than the top throws the shadow beyond the floor
  if (!lands) endX = lampX < objectX ? PHYSICS_W + 40.0f : -40.0f;
  const float limited = physicsClamp(endX, -40.0f, PHYSICS_W + 40.0f);
  const int shadowLeft = (int)(limited < objectX ? limited : (float)objectX);
  const int shadowRight = (int)(limited < objectX ? (float)objectX : limited);
  // The light ray just past the top of the object lands where the shadow ends
  c.drawLine(lx, ly, (int)limited, floorY, PHYSICS_CITRUS);
  c.fillRect(shadowLeft, floorY, shadowRight - shadowLeft, 7, PHYSICS_BLACK);
  c.fillRect(objectX - 6, floorY - (int)height, 12, (int)height, PHYSICS_BLUE);
  c.fillCircle(lx, ly, 7, PHYSICS_CITRUS);
  c.drawFastVLine(lx, 0, ly, PHYSICS_GREY);

  char text[28];
  physicsValue(text, sizeof(text), height, 0, "cm", s.german);
  physicsText(c, s.normal, text, 2, 112, PHYSICS_BLUE, textdatum_t::baseline_left);
  const float length = fabsf(endX - objectX);
  char number[16], line[32];
  if (lands && length < 500.0f) {
    physicsValue(number, sizeof(number), length, 0, "cm", s.german);
  } else {
    snprintf(number, sizeof(number), "%s", physicsPick(s, "sehr lang", "very long"));
  }
  snprintf(line, sizeof(line), "%s %s", physicsPick(s, "Schatten", "Shadow"), number);
  physicsText(c, s.normal, line, PHYSICS_W - 2, 112, PHYSICS_WHITE, textdatum_t::baseline_right);
}

// ---- Mirrors ----
template <class C>
void physicsDrawRay(C& c, float x0, float y0, float x1, float y1, float phase, uint16_t color) {
  c.drawLine((int)x0, (int)y0, (int)x1, (int)y1, color);
  c.drawLine((int)x0, (int)y0 + 1, (int)x1, (int)y1 + 1, color);
  // A pulse of light travels along the ray
  const float at = phase - floorf(phase);
  c.fillCircle((int)(x0 + (x1 - x0) * at), (int)(y0 + (y1 - y0) * at), 2, PHYSICS_WHITE);
}

template <class C>
void physicsDrawFlatMirror(C& c, PhysicsState& s) {
  const int hitX = PHYSICS_W / 2, hitY = 88;
  const float angle = (42.0f + 26.0f * sinf(s.t * 0.7f)) * PHYSICS_DEG;
  const float ray = 100.0f;
  // Screen y points down, and the normal up
  float rx, ry;
  const float dx = sinf(angle), dy = cosf(angle);
  physicsReflect(dx, dy, 0, -1, rx, ry);
  for (int y = 6; y < hitY; y += 6) c.drawFastVLine(hitX, y, 3, PHYSICS_GREY);
  physicsDrawRay(c, hitX - ray * dx, hitY - ray * dy, (float)hitX, (float)hitY, s.t, PHYSICS_CITRUS);
  physicsDrawRay(c, (float)hitX, (float)hitY, hitX + ray * rx, hitY + ray * ry, s.t, PHYSICS_ORANGE);
  c.fillRect(20, hitY + 1, PHYSICS_W - 40, 4, PHYSICS_LIGHT);
  for (int x = 24; x < PHYSICS_W - 20; x += 8) c.drawLine(x, hitY + 5, x - 4, hitY + 10, PHYSICS_GREY);
  char text[12];
  snprintf(text, sizeof(text), "%d\xC2\xB0", (int)(angle / PHYSICS_DEG + 0.5f));
  c.fillRect(0, 2, 42, 22, PHYSICS_BLACK);
  c.fillRect(PHYSICS_W - 42, 2, 42, 22, PHYSICS_BLACK);
  physicsText(c, s.big, text, 2, 22, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsText(c, s.big, text, PHYSICS_W - 2, 22, PHYSICS_WHITE, textdatum_t::baseline_right);
}

// Parallel rays on a round mirror. hollow: seen from inside the circle; otherwise from outside.
template <class C>
void physicsDrawRoundMirror(C& c, PhysicsState& s, bool hollow) {
  const float middleY = 50.0f;
  const float radius = hollow ? 150.0f : 100.0f;
  const float cx = hollow ? 60.0f : 270.0f;
  static const float offsets[4] = {-34.0f, -14.0f, 14.0f, 34.0f};
  for (int i = 0; i < 4; i++) {
    const float startY = middleY + offsets[i];
    float t;
    if (!physicsRayCircle(0, startY, 1, 0, cx, middleY, radius, hollow, t)) continue;
    const float hitX = t, hitY = startY;
    float rx, ry;
    physicsReflect(1, 0, (hitX - cx) / radius, (hitY - middleY) / radius, rx, ry);
    physicsDrawRay(c, 0, startY, hitX, hitY, s.t * 0.5f + i * 0.25f, PHYSICS_CITRUS);
    const float back = hollow ? 160.0f : 240.0f;
    physicsDrawRay(c, hitX, hitY, hitX + back * rx, hitY + back * ry, s.t * 0.5f + i * 0.25f, PHYSICS_ORANGE);
    if (!hollow) {
      // Seen in the mirror, the light seems to come from one point behind it
      for (float k = 6; k < 60; k += 8) {
        c.drawPixel((int)(hitX - k * rx), (int)(hitY - k * ry), PHYSICS_LIGHT);
      }
    }
  }
  int lastX = 0, lastY = 0;
  for (int i = 0; i <= 20; i++) {
    const int y = 2 + i * 5;
    const float dy = y - middleY;
    const float dx = sqrtf(radius * radius - dy * dy);
    const int x = (int)(hollow ? cx + dx : cx - dx);
    if (i > 0) physicsThickLine(c, lastX, lastY, x, y, PHYSICS_LIGHT);
    lastX = x;
    lastY = y;
  }
  // The burning point of the hollow mirror, or the point the rays seem to come from: half the radius
  const int focusX = (int)(hollow ? cx + radius / 2 : cx - radius / 2);
  c.fillCircle(focusX, (int)middleY, 3, PHYSICS_RED);
}

template <class C>
void physicsDrawMirror(C& c, PhysicsState& s, float dt) {
  s.t += dt;
  if (s.param == 0) physicsDrawFlatMirror(c, s);
  else physicsDrawRoundMirror(c, s, s.param == 1);
  static const char* const de[3] = {"flacher Spiegel", "Hohlspiegel", "W\xC3\xB6lbspiegel"};
  static const char* const en[3] = {"Flat mirror", "Concave mirror", "Convex mirror"};
  c.fillRect(0, 101, PHYSICS_W, PHYSICS_H - 101, PHYSICS_BLACK);
  physicsText(c, s.normal, s.german ? de[s.param] : en[s.param], 2, 112, PHYSICS_WHITE, textdatum_t::baseline_left);
}

// ---- Refraction ----
template <class C>
void physicsDrawRefraction(C& c, PhysicsState& s, float dt) {
  static const uint16_t tints[4] = {PHYSICS_BLACK, 0x0B38, 0x3A8D, 0x5ACB};
  const PhysicsLightPath& path = PHYSICS_LIGHT_PATHS[s.param];
  const int hitX = PHYSICS_W / 2, hitY = 56;
  const float ray = 90.0f;
  s.t += dt;
  const float angleIn = (38.0f + 36.0f * sinf(s.t * 0.5f)) * PHYSICS_DEG;

  c.fillRect(0, 0, PHYSICS_W, hitY, tints[path.tintUpper]);
  c.fillRect(0, hitY, PHYSICS_W, PHYSICS_H - hitY, tints[path.tintLower]);
  for (int y = 2; y < PHYSICS_H; y += 8) c.drawFastVLine(hitX, y, 4, PHYSICS_LIGHT);

  physicsThickLine(c, hitX - (int)(ray * sinf(angleIn)), hitY - (int)(ray * cosf(angleIn)), hitX, hitY, PHYSICS_CITRUS);
  float angleOut = 0;
  const bool through = physicsSnell(path.nUpper, path.nLower, angleIn, angleOut);
  if (through) {
    physicsThickLine(c, hitX, hitY, hitX + (int)(ray * sinf(angleOut)), hitY + (int)(ray * cosf(angleOut)), PHYSICS_CITRUS);
  } else {
    float rx, ry;
    physicsReflect(sinf(angleIn), cosf(angleIn), 0, -1, rx, ry);
    physicsThickLine(c, hitX, hitY, hitX + (int)(48.0f * rx), hitY + (int)(48.0f * ry), PHYSICS_CITRUS);
  }
  char number[12], text[28];
  physicsFormatNumber(number, sizeof(number), path.nUpper, 2, s.german);
  snprintf(text, sizeof(text), "%s %s", s.german ? path.upperDe : path.upperEn, number);
  physicsText(c, s.normal, text, PHYSICS_W - 2, 16, PHYSICS_WHITE, textdatum_t::baseline_right);
  snprintf(text, sizeof(text), "%d\xC2\xB0", (int)(angleIn / PHYSICS_DEG + 0.5f));
  physicsText(c, s.big, text, PHYSICS_W - 2, 42, PHYSICS_WHITE, textdatum_t::baseline_right);
  physicsFormatNumber(number, sizeof(number), path.nLower, 2, s.german);
  snprintf(text, sizeof(text), "%s %s", s.german ? path.lowerDe : path.lowerEn, number);
  physicsText(c, s.normal, text, 2, 78, PHYSICS_WHITE, textdatum_t::baseline_left);
  if (through) {
    snprintf(text, sizeof(text), "%d\xC2\xB0", (int)(angleOut / PHYSICS_DEG + 0.5f));
    physicsText(c, s.big, text, 2, 106, PHYSICS_WHITE, textdatum_t::baseline_left);
  } else {
    physicsText(c, s.normal, physicsPick(s, "Totalreflexion", "Total reflection"), 2, 104, PHYSICS_RED, textdatum_t::baseline_left);
  }
}

// ---- Sound ----
template <class C>
void physicsDrawSound(C& c, PhysicsState& s, float dt) {
  const PhysicsTone& tone = PHYSICS_TONES[s.param];
  const float perMetre = 60.0f;
  const float wavelength = physicsWavelength(PHYSICS_SOUND_SPEED, tone.hertz) * perMetre;
  // Slowed so that the eye can follow: the wave crosses the screen at the same pace for every tone,
  // because sound speed doesn't depend on pitch
  const float period = wavelength / PHYSICS_SOUND_SCREEN_SPEED;
  const float amplitude = tone.push * wavelength / (2.0f * PHYSICS_PI);  // Equal push, equal pressure
  s.t += dt;
  // Keeps the sine's argument small enough to stay exact however long it runs
  if (s.t > 64.0f * PHYSICS_SOUND_LONGEST) s.t = 0;

  const int startX = 16;
  const float cone = physicsWaveDisplacement(amplitude, wavelength, period, 0, s.t);
  c.fillRect(0, 6, 6, 40, PHYSICS_GREY);
  c.fillRect(6 + (int)cone, 6, 3, 40, PHYSICS_LIGHT);
  for (int x = startX + 8; x < PHYSICS_W; x += 5) {
    const float rel = (float)(x - startX);
    const int shifted = x + (int)physicsWaveDisplacement(amplitude, wavelength, period, rel, s.t);
    for (int row = 0; row < 5; row++) c.fillRect(shifted, 8 + row * 8, 2, 2, PHYSICS_WHITE);
  }
  // Below it, the air pressure along the way: a wave
  const int midY = 76;
  c.drawFastHLine(startX, midY, PHYSICS_W - startX, PHYSICS_DARK);
  int lastY = midY;
  for (int x = startX; x < PHYSICS_W; x += 2) {
    const int y = midY - (int)physicsWavePressure(tone.push * 24.0f, wavelength, period, (float)(x - startX), s.t);
    if (x > startX) c.drawLine(x - 2, lastY, x, y, PHYSICS_CITRUS);
    lastY = y;
  }
  char text[20];
  physicsText(c, s.normal, s.german ? tone.de : tone.en, 2, 112, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsText(c, s.normal, physicsPick(s, "Zeitlupe", "slowed"), 138, 112, PHYSICS_GREY, textdatum_t::baseline_center);
  physicsValue(text, sizeof(text), tone.hertz, 0, "Hz", s.german);
  physicsText(c, s.normal, text, PHYSICS_W - 2, 112, PHYSICS_CITRUS, textdatum_t::baseline_right);
}

// ---- Circuit ----
template <class C>
void physicsDrawBulb(C& c, int x, int y, float brightness) {
  const int r = 13;
  c.fillCircle(x, y, r, PHYSICS_BLACK);
  if (brightness > 0) {
    c.fillCircle(x, y, r, physicsShade565(PHYSICS_CITRUS, 0.25f + 0.75f * brightness));
    // Rays, as long as the bulb is bright
    const int reach = (int)(9.0f * brightness);
    for (int i = 0; i < 8 && reach > 0; i++) {
      const float a = i * PHYSICS_PI / 4.0f;
      const float cs = cosf(a), sn = sinf(a);
      if (sn > 0.5f) continue;  // Screen y points down: leave the wire below free
      c.drawLine(x + (int)((r + 2) * cs), y - (int)((r + 2) * sn), x + (int)((r + 2 + reach) * cs),
                 y - (int)((r + 2 + reach) * sn), PHYSICS_CITRUS);
    }
  }
  c.drawCircle(x, y, r, PHYSICS_LIGHT);
}

// A point on the wire loop, going round from the battery's minus through the switch and bulbs to its plus
inline void physicsWirePoint(float along, int& x, int& y) {
  const int left = 20, right = 140, top = 18, bottom = 104;
  const int width = right - left, height = bottom - top;
  const float loop = 2.0f * (width + height);
  float d = along - loop * floorf(along / loop);
  if (d < width) { x = left + (int)d; y = bottom; return; }
  d -= width;
  if (d < height) { x = right; y = bottom - (int)d; return; }
  d -= height;
  if (d < width) { x = right - (int)d; y = top; return; }
  d -= width;
  x = left;
  y = top + (int)d;
}

template <class C>
void physicsDrawCircuit(C& c, PhysicsState& s, float dt) {
  const PhysicsCircuit& circuit = PHYSICS_CIRCUITS[s.param];
  const int left = 20, right = 140, top = 18, bottom = 104;
  const float amps = circuit.closed ? physicsOhmCurrent(circuit.volts, physicsSeriesResistance(PHYSICS_BULB_OHMS, circuit.bulbs)) : 0.0f;
  // Electrons move with the current. The dots show its way and strength, not their true pace
  s.t += dt * amps * 100.0f;
  if (s.t > 416.0f) s.t -= 416.0f;

  c.drawRect(left, top, right - left + 1, bottom - top + 1, PHYSICS_LIGHT);
  for (int i = 0; i < 16; i++) {
    int x, y;
    physicsWirePoint(s.t + i * 26.0f, x, y);
    // Inside the battery there is no wire to show them on
    if (x == left && y > 42 && y < 80) continue;
    c.fillCircle(x, y, 2, PHYSICS_BLUE);
  }
  // Battery on the left: long plate plus, short plate minus, in a stack
  const int cells = circuit.volts > 5.0f ? 2 : 1;
  const int battery = (top + bottom) / 2;
  c.fillRect(left - 6, battery - 16, 13, 33, PHYSICS_BLACK);
  for (int i = 0; i < cells; i++) {
    const int y = battery - 6 * cells + i * 12 + 3;
    c.fillRect(left - 9, y - 1, 19, 3, PHYSICS_WHITE);
    c.fillRect(left - 4, y + 5, 9, 3, PHYSICS_WHITE);
  }
  physicsText(c, s.normal, "+", left + 13, battery - 8, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsText(c, s.normal, "-", left + 13, battery + 18, PHYSICS_WHITE, textdatum_t::baseline_left);
  // Switch at the bottom
  const int switchX = (left + right) / 2;
  c.fillRect(switchX - 14, bottom - 1, 28, 3, PHYSICS_BLACK);
  c.fillCircle(switchX - 14, bottom, 3, PHYSICS_WHITE);
  c.fillCircle(switchX + 14, bottom, 3, PHYSICS_WHITE);
  if (circuit.closed) physicsThickLine(c, switchX - 14, bottom, switchX + 14, bottom, PHYSICS_WHITE);
  else physicsThickLine(c, switchX - 14, bottom, switchX + 10, bottom - 14, PHYSICS_WHITE);
  // Bulbs on top: as bright as the power each one gets
  const float power = physicsPower(amps, PHYSICS_BULB_OHMS);
  const float brightest = physicsPower(physicsOhmCurrent(9.0f, PHYSICS_BULB_OHMS), PHYSICS_BULB_OHMS);
  const float brightness = sqrtf(power / brightest);
  if (circuit.bulbs == 1) {
    physicsDrawBulb(c, switchX, top, brightness);
  } else {
    physicsDrawBulb(c, switchX - 28, top, brightness);
    physicsDrawBulb(c, switchX + 28, top, brightness);
  }
  char text[20];
  const int textX = 160;
  physicsText(c, s.big, circuit.closed ? physicsPick(s, "AN", "ON") : physicsPick(s, "AUS", "OFF"), textX, 30,
              circuit.closed ? PHYSICS_GREEN : PHYSICS_RED, textdatum_t::baseline_left);
  physicsValue(text, sizeof(text), circuit.volts, 1, "V", s.german);
  physicsText(c, s.big, text, textX, 66, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsValue(text, sizeof(text), amps, 2, "A", s.german);
  physicsText(c, s.big, text, textX, 100, PHYSICS_CITRUS, textdatum_t::baseline_left);
}

// ---- States of matter ----
template <class C>
void physicsDrawMatter(C& c, PhysicsState& s, float dt) {
  static const uint16_t colors[3] = {PHYSICS_BLUE, PHYSICS_GREEN, PHYSICS_RED};
  static const float targets[3] = {PHYSICS_MATTER_SOLID_T, PHYSICS_MATTER_LIQUID_T, PHYSICS_MATTER_GAS_T};
  static const char* const de[3] = {"fest", "fl\xC3\xBCssig", "gasf\xC3\xB6rmig"};
  static const char* const en[3] = {"solid", "liquid", "gas"};
  static const char* const heatDe[3] = {"kalt", "warm", "heiss"};
  static const char* const heatEn[3] = {"cold", "warm", "hot"};
  const int boxX = 6, boxY = 2;
  const float perUnit = 228.0f / PHYSICS_MATTER_WIDTH;
  const int boxHeight = (int)(PHYSICS_MATTER_HEIGHT * perUnit);
  s.t += dt;
  // One unit of the model's time per second; a long frame can't make it run away
  int steps = (int)(s.t / PHYSICS_MATTER_DT);
  s.t -= steps * PHYSICS_MATTER_DT;
  if (steps > 24) steps = 24;
  for (int i = 0; i < steps; i++) physicsMatterStep(s.matter, targets[s.param]);

  c.drawRect(boxX - 1, boxY - 1, 230, boxHeight + 2, colors[s.param]);
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) {
    const int x = boxX + (int)(s.matter.x[i] * perUnit);
    const int y = boxY + boxHeight - (int)(s.matter.y[i] * perUnit);
    c.fillCircle(x, y, 5, colors[s.param]);
  }
  physicsText(c, s.normal, s.german ? de[s.param] : en[s.param], 4, 112, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsText(c, s.normal, s.german ? heatDe[s.param] : heatEn[s.param], PHYSICS_W - 4, 112, colors[s.param], textdatum_t::baseline_right);
}

// ---- Seasons ----
template <class C>
void physicsDrawSeasons(C& c, PhysicsState& s, float dt) {
  static const char* const de[4] = {"Fr\xC3\xBChling", "Sommer", "Herbst", "Winter"};
  static const char* const en[4] = {"Spring", "Summer", "Autumn", "Winter"};
  // s.a: where the Earth is in the year now, s.b: where it is heading, s.t: time of day
  float gap = s.b - s.a;
  if (gap < 0) gap += 2.0f * PHYSICS_PI;
  const float move = 80.0f * PHYSICS_DEG * dt;
  if (gap <= move) s.a = s.b;
  else s.a += move;
  if (s.a >= 2.0f * PHYSICS_PI) s.a -= 2.0f * PHYSICS_PI;
  s.t += dt / 8.0f;  // A day takes eight seconds
  if (s.t >= 1.0f) s.t -= 1.0f;

  const float declination = physicsSunDeclination(s.a);
  const float hours = physicsDayHours(PHYSICS_HOME_LATITUDE, declination);
  const int cx = 124, cy = 52, r = 38;
  c.fillCircle(-6, cy, 28, PHYSICS_CITRUS);
  for (int y = cy - 24; y <= cy + 24; y += 12) c.drawFastHLine(26, y, cx - r - 30, PHYSICS_CITRUS);

  float axis[3];
  physicsEarthAxis(s.a, axis);
  const int reach = r + 11;
  physicsThickLine(c, cx - (int)(axis[0] * reach), cy + (int)(axis[1] * reach), cx + (int)(axis[0] * reach),
                   cy - (int)(axis[1] * reach), PHYSICS_WHITE);
  // Day side towards the Sun on the left, night side away from it
  for (int y = -r; y <= r; y++) {
    const int half = (int)sqrtf((float)(r * r - y * y));
    c.drawFastHLine(cx - half, cy + y, half, PHYSICS_BLUE);
    c.drawFastHLine(cx, cy + y, half + 1, (uint16_t)0x10A6);
  }
  // Home's circle of latitude, front half: yellow in sunlight, grey at night. Its lit share is the day.
  const int segments = 48;
  int lastX = 0, lastY = 0;
  bool lastSeen = false;
  for (int i = 0; i <= segments; i++) {
    float p[3];
    physicsLatitudePoint(axis, PHYSICS_HOME_LATITUDE, 2.0f * PHYSICS_PI * i / segments, p);
    const int x = cx + (int)(p[0] * r), y = cy - (int)(p[1] * r);
    const bool seen = p[2] > 0;
    if (seen && lastSeen) c.drawLine(lastX, lastY, x, y, p[0] < 0 ? PHYSICS_CITRUS : PHYSICS_GREY);
    lastX = x;
    lastY = y;
    lastSeen = seen;
  }
  // Home goes around once a day
  float home[3];
  physicsLatitudePoint(axis, PHYSICS_HOME_LATITUDE, 2.0f * PHYSICS_PI * s.t, home);
  if (home[2] > 0) c.fillCircle(cx + (int)(home[0] * r), cy - (int)(home[1] * r), 4, PHYSICS_RED);

  const int dayRounded = (int)(hours + 0.5f);
  char text[12];
  physicsText(c, s.big, s.german ? de[physicsSeasonIndex(s.a)] : en[physicsSeasonIndex(s.a)], 2, 20, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsText(c, s.normal, "47\xC2\xB0 N", 2, 94, PHYSICS_RED, textdatum_t::baseline_left);
  const int textX = 170;
  physicsText(c, s.normal, physicsPick(s, "Tag", "Day"), textX, 18, PHYSICS_CITRUS, textdatum_t::baseline_left);
  snprintf(text, sizeof(text), "%d h", dayRounded);
  physicsText(c, s.big, text, textX, 44, PHYSICS_CITRUS, textdatum_t::baseline_left);
  physicsText(c, s.normal, physicsPick(s, "Nacht", "Night"), textX, 66, PHYSICS_LIGHT, textdatum_t::baseline_left);
  snprintf(text, sizeof(text), "%d h", 24 - dayRounded);
  physicsText(c, s.big, text, textX, 92, PHYSICS_LIGHT, textdatum_t::baseline_left);
  // A day of 24 hours as a bar: noon in the middle
  const int barWidth = PHYSICS_W - 8;
  const int lit = (int)(barWidth * hours / 24.0f);
  c.fillRect(4, 102, barWidth, 11, (uint16_t)0x10A6);
  c.fillRect(4 + (barWidth - lit) / 2, 102, lit, 11, PHYSICS_CITRUS);
}

// ---- Moon phases ----
template <class C>
void physicsDrawLitDisc(C& c, int cx, int cy, int r, float lit, bool growing, uint16_t light, uint16_t dark) {
  c.fillCircle(cx, cy, r, dark);
  for (int y = -r; y <= r; y++) {
    float from, to;
    physicsLitSpan((float)y, (float)r, lit, growing, from, to);
    const int x0 = (int)floorf(from + 0.5f), x1 = (int)floorf(to + 0.5f);
    if (x1 > x0) c.drawFastHLine(cx + x0, cy + y, x1 - x0, light);
  }
}

template <class C>
void physicsDrawMoon(C& c, PhysicsState& s, float dt) {
  static const char* const de[8] = {"Neumond", "Zunehmende Sichel", "Erstes Viertel", "Zunehmender Mond",
                                    "Vollmond", "Abnehmender Mond", "Letztes Viertel", "Abnehmende Sichel"};
  static const char* const en[8] = {"New moon", "Waxing crescent", "First quarter", "Waxing gibbous",
                                    "Full moon", "Waning gibbous", "Last quarter", "Waning crescent"};
  const uint16_t light = 0xFFD9, dark = 0x2945;
  // Today stands still. Otherwise, or with no clock to know today, the month runs: two days a second
  if (s.param == 1 || !s.clockKnown) {
    s.t += dt * 2.0f;
    if (s.t >= 30.0f) s.t -= 30.0f;
  } else {
    s.t = 0;
  }
  const double day = s.startDay + s.t;
  const float elongation = physicsMoonElongation(day);
  const float lit = physicsMoonIllumination(elongation);
  const bool growing = elongation < 180.0f;
  // The Moon as we see it from here in the north
  physicsDrawLitDisc(c, 48, 50, 44, lit, growing, light, dark);

  // The same seen from high above: the Sun far off to the right lights one half of Earth and Moon
  const int ex = 166, ey = 58, orbit = 36;
  c.drawCircle(ex, ey, orbit, PHYSICS_DARK);
  physicsDrawLitDisc(c, ex, ey, 8, 0.5f, true, PHYSICS_BLUE, (uint16_t)0x10A6);
  const float a = elongation * PHYSICS_DEG;
  physicsDrawLitDisc(c, ex + (int)(orbit * cosf(a)), ey - (int)(orbit * sinf(a)), 6, 0.5f, true, light, dark);
  c.fillCircle(PHYSICS_W - 1, ey, 13, PHYSICS_CITRUS);

  physicsText(c, s.normal, s.german ? de[physicsMoonPhaseIndex(elongation)] : en[physicsMoonPhaseIndex(elongation)], 2, 112,
              PHYSICS_WHITE, textdatum_t::baseline_left);
  if (s.clockKnown) {
    char text[16];
    const int days = (int)s.t;
    if (s.param == 0) snprintf(text, sizeof(text), "%s", physicsPick(s, "heute", "today"));
    else snprintf(text, sizeof(text), "+%d %s", days, days == 1 ? physicsPick(s, "Tag", "day") : physicsPick(s, "Tage", "days"));
    physicsText(c, s.normal, text, PHYSICS_W - 2, 15, PHYSICS_CITRUS, textdatum_t::baseline_right);
  }
}

// ---- The list ----

// Starts the current scene over with the current setting
inline void physicsSceneReset(PhysicsState& s) {
  s.t = s.a = s.b = s.c = 0;
  switch (s.scene) {
    case PHYSICS_BUOYANCY:
      s.a = -0.04f;
      break;
    case PHYSICS_PENDULUM:
      s.a = PHYSICS_PENDULUM_START;
      break;
    case PHYSICS_RAMP:
      s.a = PHYSICS_RAMP_START;
      break;
    case PHYSICS_MAGNET:
      physicsMagnetBuild(s);
      break;
    case PHYSICS_MATTER:
      physicsMatterInit(s.matter);
      break;
    case PHYSICS_SEASONS:
      // Today's place in the year; with no clock, midsummer
      s.a = s.clockKnown ? physicsSunLongitude(s.startDay) : PHYSICS_PI / 2.0f;
      s.b = s.a;
      break;
    default:
      break;
  }
}

inline void physicsSceneNextParam(PhysicsState& s) {
  const uint8_t count = PHYSICS_SCENES[s.sceneIndex].paramCount;
  s.param = (uint8_t)((s.param + 1) % count);
  // These carry on from where they are, so you see the change happen
  if (s.scene == PHYSICS_LEVER || s.scene == PHYSICS_MATTER) return;
  if (s.scene == PHYSICS_SEASONS) {
    s.b = physicsNextSeasonLongitude(s.b);
    return;
  }
  physicsSceneReset(s);
}

template <class C>
void physicsDrawFrame(C& c, PhysicsState& s, float dt) {
  c.fillSprite(PHYSICS_BLACK);
  switch (s.scene) {
    case PHYSICS_FALLING: physicsDrawFalling(c, s, dt); break;
    case PHYSICS_FRICTION: physicsDrawFriction(c, s, dt); break;
    case PHYSICS_LEVER: physicsDrawLever(c, s, dt); break;
    case PHYSICS_PULLEY: physicsDrawPulley(c, s, dt); break;
    case PHYSICS_BUOYANCY: physicsDrawBuoyancy(c, s, dt); break;
    case PHYSICS_PENDULUM: physicsDrawPendulum(c, s, dt); break;
    case PHYSICS_RAMP: physicsDrawRamp(c, s, dt); break;
    case PHYSICS_MAGNET: physicsDrawMagnet(c, s, dt); break;
    case PHYSICS_SHADOW: physicsDrawShadow(c, s, dt); break;
    case PHYSICS_MIRROR: physicsDrawMirror(c, s, dt); break;
    case PHYSICS_REFRACTION: physicsDrawRefraction(c, s, dt); break;
    case PHYSICS_SOUND: physicsDrawSound(c, s, dt); break;
    case PHYSICS_CIRCUIT: physicsDrawCircuit(c, s, dt); break;
    case PHYSICS_MATTER: physicsDrawMatter(c, s, dt); break;
    case PHYSICS_SEASONS: physicsDrawSeasons(c, s, dt); break;
    case PHYSICS_MOON: physicsDrawMoon(c, s, dt); break;
  }
}
