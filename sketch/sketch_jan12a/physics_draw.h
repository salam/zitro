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
    default:
      break;
  }
}

inline void physicsSceneNextParam(PhysicsState& s) {
  const uint8_t count = PHYSICS_SCENES[s.sceneIndex].paramCount;
  s.param = (uint8_t)((s.param + 1) % count);
  // The lever keeps its tilt: you see it move from the old load to the new one
  if (s.scene != PHYSICS_LEVER) physicsSceneReset(s);
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
  }
}
