// Host tests for the physics math. Build and run: tests/physics/run.sh
// Moon and season dates are from the US Naval Observatory: aa.usno.navy.mil/api, fetched 2026-11-06.
#include <string>
#include <vector>
#include "../../sketch/sketch_jan12a/physics_atom_table.h"
#include "../../sketch/sketch_jan12a/physics_core.h"
#include "test_common.h"

static void testFalling() {
  NEAR(physicsFallDistance(9.81f, 1.0f), 4.905, 1e-4);
  NEAR(physicsFallSpeed(9.81f, 2.0f), 19.62, 1e-4);
  NEAR(physicsFallTime(9.81f, 5.0f), 1.0096, 1e-3);
  NEAR(physicsFallTime(1.62f, 5.0f), 2.4845, 1e-3);
}

static void testFriction() {
  NEAR(physicsStopDistance(2.0f, 0.03f, 9.81f), 6.796, 1e-2);
  NEAR(physicsStopDistance(2.0f, 0.8f, 9.81f), 0.2548, 1e-3);
  const float stop = physicsStopTime(2.0f, 0.3f, 9.81f);
  NEAR(physicsSlideDistance(2.0f, 0.3f, 9.81f, 0.0f), 0.0, 1e-5);
  NEAR(physicsSlideDistance(2.0f, 0.3f, 9.81f, stop), physicsStopDistance(2.0f, 0.3f, 9.81f), 1e-3);
  // Stays stopped: friction doesn't push it back
  NEAR(physicsSlideDistance(2.0f, 0.3f, 9.81f, stop + 10), physicsStopDistance(2.0f, 0.3f, 9.81f), 1e-3);
}

static void testLever() {
  CHECK(physicsLeverSide(2, 2, 1, 4) == 0);
  CHECK(physicsLeverSide(2, 2, 2, 2) == 0);
  CHECK(physicsLeverSide(2, 2, 4, 1) == 0);
  CHECK(physicsLeverSide(2, 2, 1, 2) == -1);
  CHECK(physicsLeverSide(2, 2, 2, 4) == 1);
  struct { float m, d; int side; } cases[] = {{1, 4, 0}, {4, 1, 0}, {1, 2, -1}, {2, 4, 1}, {1, 1, -1}, {4, 4, 1}};
  for (auto& k : cases) {
    // Whatever the start, the beam ends level if balanced, otherwise on the heavier side
    for (float start : {-0.2f, 0.0f, 0.2f}) {
      float angle = start, omega = 0;
      for (int i = 0; i < 40000; i++) physicsLeverStep(angle, omega, 2, 2, k.m, k.d, 9.81f, 0.001f);
      if (k.side == 0) NEAR(angle, 0, 0.002);
      else NEAR(angle, k.side * PHYSICS_LEVER_MAX_ANGLE, 1e-4);
    }
  }
}

static void testPulley() {
  NEAR(physicsPulleyForce(100, 1), 100, 1e-5);
  NEAR(physicsPulleyForce(100, 2), 50, 1e-5);
  NEAR(physicsPulleyForce(100, 4), 25, 1e-5);
  NEAR(physicsPulleyForce(100, 0), 100, 1e-5);
  NEAR(physicsPulleyRope(15, 4), 60, 1e-5);
  // Work in equals work out: you can't get something for nothing
  for (int n : {1, 2, 4}) NEAR(physicsPulleyForce(100, n) * physicsPulleyRope(15, n), 1500, 1e-3);
}

static void testBuoyancy() {
  CHECK(physicsFloats(0.24f, 1.0f));
  CHECK(physicsFloats(0.92f, 1.0f));
  CHECK(!physicsFloats(2.7f, 1.0f));
  CHECK(!physicsFloats(7.87f, 1.0f));
  NEAR(physicsFloatFraction(0.92f, 1.0f), 0.92, 1e-5);  // Most of an iceberg is under water
  NEAR(physicsFloatFraction(7.87f, 1.0f), 1.0, 1e-5);
  const float floorDepth = 0.37f;
  for (float density : {0.24f, 0.6f, 0.92f}) {
    float depth = -0.12f, speed = 0;
    for (int i = 0; i < 60000; i++) physicsBuoyancyStep(depth, speed, density, floorDepth, 9.81f, 0.001f);
    NEAR(depth / PHYSICS_CUBE_SIDE, density, 0.01);  // Settles at the fraction Archimedes gives
    NEAR(speed, 0, 0.01);
  }
  for (float density : {2.7f, 7.87f}) {
    float depth = -0.12f, speed = 0;
    for (int i = 0; i < 10000; i++) physicsBuoyancyStep(depth, speed, density, floorDepth, 9.81f, 0.001f);
    NEAR(depth, floorDepth, 1e-5);
  }
}

static void testMagnet() {
  PhysicsPole north = {0, 0, 1};
  float bx, by, bx2, by2;
  physicsPoleField(&north, 1, 10, 0, bx, by);
  physicsPoleField(&north, 1, 20, 0, bx2, by2);
  CHECK(bx > 0);  // Away from north
  NEAR(by, 0, 1e-7);
  NEAR(bx / bx2, 4.0, 1e-3);  // Square of the distance
  physicsPoleField(&north, 1, 0, 0, bx, by);  // On the pole itself: no crash, no infinity
  CHECK(isfinite(bx) && isfinite(by));

  PhysicsPole bar[2] = {{-20, 0, 1}, {20, 0, -1}};
  physicsPoleField(bar, 2, 0, 0, bx, by);
  CHECK(bx > 0);  // Between the poles: from north to south
  NEAR(by, 0, 1e-6);

  PhysicsPole repel[2] = {{-20, 0, 1}, {20, 0, 1}};
  physicsPoleField(repel, 2, 0, 0, bx, by);
  NEAR(bx, 0, 1e-6);  // Exactly between like poles the forces cancel
  NEAR(by, 0, 1e-6);

  // Every line that starts at north ends at south, given room for the wide loops
  static int16_t xs[8000], ys[8000];
  int arrived = 0, lines = 12;
  for (int i = 0; i < lines; i++) {
    const float a = 2 * PHYSICS_PI * (i + 0.5f) / lines;
    int n = physicsTraceFieldLine(bar, 2, -20 + 3 * cosf(a), 3 * sinf(a), 2.0f, -5000, -5000, 5000, 5000, xs, ys, 8000);
    CHECK(n > 1);
    const float dx = xs[n - 1] - 20.0f, dy = ys[n - 1];
    if (dx * dx + dy * dy < 16) arrived++;
  }
  CHECK(arrived == lines);
  // Small buffers and tight bounds are respected
  CHECK(physicsTraceFieldLine(bar, 2, 0, 5, 2.0f, -500, -500, 500, 500, xs, ys, 3) == 3);
  CHECK(physicsTraceFieldLine(bar, 2, 0, 5, 2.0f, 10, 10, 20, 20, xs, ys, 400) == 1);
  CHECK(physicsTraceFieldLine(repel, 2, 0, 0, 2.0f, -500, -500, 500, 500, xs, ys, 400) == 1);  // No field, no line
  CHECK(physicsTraceFieldLine(bar, 2, 0, 5, 2.0f, -500, -500, 500, 500, xs, ys, 0) == 0);
}

// Time for the swing to come back, by watching the simulation itself
static float measuredPeriod(float length, float amplitude, float g) {
  float theta = amplitude, omega = 0, previous = 0, t = 0;
  const float dt = 0.001f;
  int crossings = 0;
  float first = 0;
  while (t < 60 && crossings < 3) {
    previous = omega;
    physicsPendulumStep(theta, omega, length, g, dt);
    t += dt;
    // omega changes sign at each end of the swing; every second one is a full period
    if ((previous > 0 && omega <= 0) || (previous < 0 && omega >= 0)) {
      if (crossings == 0) first = t;
      crossings++;
    }
  }
  return t - first;
}

static void testPendulum() {
  NEAR(physicsPendulumPeriod(1.0f, 9.81f), 2.006, 1e-3);
  NEAR(physicsPendulumPeriod(0.25f, 9.81f), 1.003, 1e-3);
  NEAR(physicsPendulumPeriod(2.25f, 9.81f), 3.009, 1e-3);
  NEAR(physicsPendulumPeriod(4.0f, 9.81f) / physicsPendulumPeriod(1.0f, 9.81f), 2.0, 1e-4);
  NEAR(physicsPendulumPeriod(1.0f, 1.62f), 4.937, 1e-3);  // Slower on the Moon
  // Small swing: the simulation matches the formula
  for (float length : {0.25f, 1.0f, 2.25f}) {
    NEAR(measuredPeriod(length, 2 * PHYSICS_DEG, 9.81f), physicsPendulumPeriod(length, 9.81f), 0.005);
  }
  // Large swing: slightly slower, as the textbook series says (1 + a^2/16 + 11 a^4/3072)
  const float a = 25 * PHYSICS_DEG;
  const float series = physicsPendulumPeriod(1.0f, 9.81f) * (1 + a * a / 16 + 11 * a * a * a * a / 3072);
  NEAR(measuredPeriod(1.0f, a, 9.81f), series, 0.005);
  // Weight isn't in the equation at all; energy doesn't drift
  float theta = a, omega = 0;
  for (int i = 0; i < 120000; i++) physicsPendulumStep(theta, omega, 1.0f, 9.81f, 0.001f);
  const float energy = 0.5f * omega * omega + 9.81f * (1 - cosf(theta));
  NEAR(energy, 9.81f * (1 - cosf(a)), 1e-3);
}

static void testRamp() {
  const float a = 1.0f, g = 9.81f;
  float x = -0.95f, v = 0;
  const float start = physicsRampHeightEnergy(x, a, g);
  float highest = 0, fastest = 0;
  for (int i = 0; i < 60000; i++) {  // A minute
    physicsRampStep(x, v, a, g, 0, 0.001f);
    const float e = physicsRampHeightEnergy(x, a, g) + physicsRampMotionEnergy(x, v, a);
    NEAR(e, start, 0.01f * start);  // No friction: the sum stays
    if (fabsf(x) < 0.01f) fastest = fmaxf(fastest, fabsf(v));
    highest = fmaxf(highest, fabsf(x));
  }
  NEAR(highest, 0.95, 0.01);                     // Back up to the same height
  NEAR(fastest, sqrtf(2 * start), 0.03);         // All height turned to speed at the bottom
  x = -0.95f; v = 0;
  float before = start;
  bool falling = true;
  for (int i = 0; i < 30000; i++) {
    physicsRampStep(x, v, a, g, 0.6f, 0.001f);
    const float e = physicsRampHeightEnergy(x, a, g) + physicsRampMotionEnergy(x, v, a);
    if (e > before + 1e-5f) falling = false;
    before = e;
  }
  CHECK(falling);  // With friction the sum only goes down; the rest is heat
  CHECK(before < 0.01f * start);
}

static void testShadow() {
  float end = 0;
  CHECK(physicsShadowEnd(0, 40, 0, 20, end));
  NEAR(end, 0, 1e-5);  // Lamp right above: no shadow to the side
  CHECK(physicsShadowEnd(-30, 40, 0, 20, end));
  NEAR(end, 30, 1e-5);  // Similar triangles
  CHECK(physicsShadowEnd(30, 40, 0, 20, end));
  NEAR(end, -30, 1e-5);  // Other side
  float low, high;
  CHECK(physicsShadowEnd(-30, 30, 0, 20, low));
  CHECK(physicsShadowEnd(-30, 80, 0, 20, high));
  CHECK(low > high);  // A lower lamp, a longer shadow
  float shorter, taller;
  CHECK(physicsShadowEnd(-30, 80, 0, 10, shorter));
  CHECK(physicsShadowEnd(-30, 80, 0, 40, taller));
  CHECK(taller > shorter);
  CHECK(!physicsShadowEnd(-30, 20, 0, 20, end));  // Lamp no higher than the top: never lands
  CHECK(!physicsShadowEnd(-30, 10, 0, 20, end));
}

static void testMirror() {
  float rx, ry;
  physicsReflect(1, 0, -1, 0, rx, ry);
  NEAR(rx, -1, 1e-6);  // Straight at the mirror: straight back
  NEAR(ry, 0, 1e-6);
  for (float deg = 5; deg < 90; deg += 5) {
    const float a = deg * PHYSICS_DEG;
    physicsReflect(sinf(a), -cosf(a), 0, 1, rx, ry);  // Mirror on the floor, normal up
    NEAR(rx, sinf(a), 1e-5);  // Angle out equals angle in, on the far side of the normal
    NEAR(ry, cosf(a), 1e-5);
    NEAR(rx * rx + ry * ry, 1, 1e-5);
  }
  float t;
  CHECK(!physicsRayCircle(0, 50, 1, 0, 100, 0, 10, false, t));  // Misses
  CHECK(physicsRayCircle(0, 0, 1, 0, 100, 0, 10, false, t));
  NEAR(t, 90, 1e-4);
  CHECK(physicsRayCircle(100, 0, 1, 0, 100, 0, 10, true, t));
  NEAR(t, 10, 1e-4);  // From inside, the far crossing
  // A hollow mirror gathers rays near its axis at half the radius
  const float radius = 140, cx = 60, cy = 58;
  for (float offset : {2.0f, 5.0f, -5.0f}) {
    CHECK(physicsRayCircle(0, cy + offset, 1, 0, cx, cy, radius, true, t));
    const float hx = t, hy = cy + offset;
    physicsReflect(1, 0, (hx - cx) / radius, (hy - cy) / radius, rx, ry);
    const float back = (cy - hy) / ry;
    NEAR(hx + back * rx, cx + radius / 2, 0.2);
  }
}

static void testRefraction() {
  float out = 0;
  CHECK(physicsSnell(1.0f, 1.33f, 30 * PHYSICS_DEG, out));
  NEAR(out / PHYSICS_DEG, 22.08, 0.02);  // Textbook example
  CHECK(physicsSnell(1.0f, 1.5f, 45 * PHYSICS_DEG, out));
  NEAR(out / PHYSICS_DEG, 28.13, 0.02);
  CHECK(physicsSnell(1.0f, 1.5f, 0, out));
  NEAR(out, 0, 1e-6);  // Straight in: no bend
  CHECK(physicsSnell(1.0f, 1.0f, 40 * PHYSICS_DEG, out));
  NEAR(out / PHYSICS_DEG, 40, 1e-3);  // Same material: no bend
  // Round trip
  CHECK(physicsSnell(1.0f, 2.42f, 60 * PHYSICS_DEG, out));
  float back;
  CHECK(physicsSnell(2.42f, 1.0f, out, back));
  NEAR(back / PHYSICS_DEG, 60, 0.02);
  // Total reflection from water to air
  NEAR(physicsCriticalAngle(1.33f, 1.0f) / PHYSICS_DEG, 48.75, 0.02);
  NEAR(physicsCriticalAngle(1.5f, 1.0f) / PHYSICS_DEG, 41.81, 0.02);
  CHECK(physicsCriticalAngle(1.0f, 1.33f) < 0);
  CHECK(physicsSnell(1.33f, 1.0f, 48 * PHYSICS_DEG, out));
  CHECK(!physicsSnell(1.33f, 1.0f, 50 * PHYSICS_DEG, out));
}

static void testSound() {
  NEAR(physicsWavelength(343, 220), 1.559, 1e-3);
  NEAR(physicsWavelength(343, 880), 0.3898, 1e-3);
  NEAR(physicsWaveDisplacement(2, 4, 1, 0, 0), 0, 1e-5);
  NEAR(physicsWaveDisplacement(2, 4, 1, 1, 0), 2, 1e-5);
  // A wave moves: the same value one wavelength and one period on
  NEAR(physicsWaveDisplacement(2, 4, 1, 1.3f, 0.2f), physicsWaveDisplacement(2, 4, 1, 1.3f + 4, 0.2f), 1e-4);
  NEAR(physicsWaveDisplacement(2, 4, 1, 1.3f, 0.2f), physicsWaveDisplacement(2, 4, 1, 1.3f, 1.2f), 1e-4);
  // Moving at wavelength / period, to the right
  NEAR(physicsWaveDisplacement(2, 4, 1, 1.3f, 0.2f), physicsWaveDisplacement(2, 4, 1, 1.3f + 0.4f, 0.3f), 1e-4);
}

static void testCircuit() {
  NEAR(physicsOhmCurrent(4.5f, 15), 0.3, 1e-5);
  NEAR(physicsOhmCurrent(9, 15), 0.6, 1e-5);
  NEAR(physicsOhmCurrent(4.5f, physicsSeriesResistance(15, 2)), 0.15, 1e-5);
  NEAR(physicsOhmCurrent(4.5f, 0), 0, 1e-5);
  NEAR(physicsPower(0.3f, 15), 1.35, 1e-4);
}

static float matterMeanHeight(const PhysicsMatter& m) {
  float sum = 0;
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) sum += m.y[i];
  return sum / PHYSICS_MATTER_COUNT;
}

// How far particles move from where they were, and how spread out they are
static void matterMeasure(float target, float& wander, float& height, float& temperature) {
  static PhysicsMatter m;
  physicsMatterInit(m);
  for (int i = 0; i < 6000; i++) physicsMatterStep(m, target);  // Reach the temperature
  float x0[PHYSICS_MATTER_COUNT], y0[PHYSICS_MATTER_COUNT];
  memcpy(x0, m.x, sizeof(x0));
  memcpy(y0, m.y, sizeof(y0));
  height = temperature = 0;
  const int steps = 4000;
  for (int i = 0; i < steps; i++) {
    physicsMatterStep(m, target);
    height += matterMeanHeight(m) / steps;
    temperature += physicsMatterTemperature(m) / steps;
  }
  wander = 0;
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) {
    wander += hypotf(m.x[i] - x0[i], m.y[i] - y0[i]) / PHYSICS_MATTER_COUNT;
    CHECK(isfinite(m.x[i]) && isfinite(m.y[i]));
    CHECK(m.x[i] >= 0 && m.x[i] <= PHYSICS_MATTER_WIDTH);
    CHECK(m.y[i] >= 0 && m.y[i] <= PHYSICS_MATTER_HEIGHT);
  }
}

static void testMatter() {
  float wS, hS, tS, wL, hL, tL, wG, hG, tG;
  matterMeasure(PHYSICS_MATTER_SOLID_T, wS, hS, tS);
  matterMeasure(PHYSICS_MATTER_LIQUID_T, wL, hL, tL);
  matterMeasure(PHYSICS_MATTER_GAS_T, wG, hG, tG);
  printf("matter: solid wander %.2f height %.2f T %.2f; liquid %.2f %.2f %.2f; gas %.2f %.2f %.2f\n", wS, hS, tS, wL, hL,
         tL, wG, hG, tG);
  NEAR(tS, PHYSICS_MATTER_SOLID_T, 0.3 * PHYSICS_MATTER_SOLID_T);
  NEAR(tL, PHYSICS_MATTER_LIQUID_T, 0.3 * PHYSICS_MATTER_LIQUID_T);
  NEAR(tG, PHYSICS_MATTER_GAS_T, 0.3 * PHYSICS_MATTER_GAS_T);
  CHECK(wS < 0.4f);      // Solid: particles stay in place
  CHECK(wL > 2 * wS);    // Liquid: they slide past each other
  CHECK(hL < 0.55f * PHYSICS_MATTER_HEIGHT * 0.8f);  // ...and it stays low in the box
  CHECK(wG > wL);        // Gas: they fly around
  CHECK(hG > hL);        // ...and fill the box
  CHECK(hG > 0.35f * PHYSICS_MATTER_HEIGHT);
}

static void testJulianDay() {
  NEAR(physicsJulianDay(2000, 1, 1, 12), 2451545.0, 1e-6);     // J2000
  NEAR(physicsJulianDay(1970, 1, 1, 0), 2440587.5, 1e-6);      // Unix epoch
  NEAR(physicsJulianDay(1987, 6, 19, 12), 2446966.0, 1e-6);    // Meeus's example
  NEAR(physicsJulianDayFromUnix(0), 2440587.5, 1e-6);
  NEAR(physicsJulianDayFromUnix(946728000.0), 2451545.0, 1e-6);
}

static void testSeasons() {
  // Equinoxes and solstices of 2026, USNO
  struct { int m, d, hh, mm; float longitude; } known[] = {
      {3, 20, 14, 46, 0}, {6, 21, 8, 24, 90}, {9, 23, 0, 5, 180}, {12, 21, 20, 50, 270}};
  for (auto& k : known) {
    const float deg = physicsSunLongitude(physicsJulianDay(2026, k.m, k.d, k.hh + k.mm / 60.0)) / PHYSICS_DEG;
    float difference = fabsf(deg - k.longitude);
    if (difference > 180) difference = 360 - difference;
    NEAR(difference, 0, 0.05);  // A twentieth of a degree: about an hour
  }
  NEAR(physicsSunDeclination(0), 0, 1e-5);
  NEAR(physicsSunDeclination(PHYSICS_PI / 2) / PHYSICS_DEG, 23.44, 1e-3);
  NEAR(physicsSunDeclination(3 * PHYSICS_PI / 2) / PHYSICS_DEG, -23.44, 1e-3);
  const float summer = PHYSICS_EARTH_TILT, winter = -PHYSICS_EARTH_TILT;
  NEAR(physicsDayHours(PHYSICS_HOME_LATITUDE, 0), 12, 0.01);  // Equal day and night
  NEAR(physicsDayHours(PHYSICS_HOME_LATITUDE, summer), 15.7, 0.1);
  NEAR(physicsDayHours(PHYSICS_HOME_LATITUDE, winter), 8.3, 0.1);
  NEAR(physicsDayHours(0, summer), 12, 0.01);  // At the equator always
  NEAR(physicsDayHours(80 * PHYSICS_DEG, summer), 24, 0.01);  // Midnight sun
  NEAR(physicsDayHours(80 * PHYSICS_DEG, winter), 0, 0.01);   // Polar night
  NEAR(physicsNoonHeight(PHYSICS_HOME_LATITUDE, summer) / PHYSICS_DEG, 66.44, 0.01);
  NEAR(physicsNoonHeight(PHYSICS_HOME_LATITUDE, winter) / PHYSICS_DEG, 19.56, 0.01);
  CHECK(physicsSeasonIndex(10 * PHYSICS_DEG) == 0);
  CHECK(physicsSeasonIndex(100 * PHYSICS_DEG) == 1);
  CHECK(physicsSeasonIndex(190 * PHYSICS_DEG) == 2);
  CHECK(physicsSeasonIndex(280 * PHYSICS_DEG) == 3);
  CHECK(physicsSeasonIndex(-1.0f) == 0);
  CHECK(physicsSeasonIndex(100.0f) == 3);
  // The drawn Earth tells the same story as the formula: the lit share of the circle of
  // latitude is the share of the day with the Sun up
  for (float longDeg : {0.0f, 45.0f, 90.0f, 200.0f, 270.0f}) {
    const float longitude = longDeg * PHYSICS_DEG;
    float axis[3];
    physicsEarthAxis(longitude, axis);
    NEAR(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2], 1, 1e-5);
    const int samples = 3600;
    int lit = 0;
    for (int i = 0; i < samples; i++) {
      float p[3];
      physicsLatitudePoint(axis, PHYSICS_HOME_LATITUDE, 2 * PHYSICS_PI * i / samples, p);
      NEAR(p[0] * axis[0] + p[1] * axis[1] + p[2] * axis[2], sinf(PHYSICS_HOME_LATITUDE), 1e-4);
      if (p[0] < 0) lit++;  // The Sun is to the left
    }
    NEAR(24.0f * lit / samples, physicsDayHours(PHYSICS_HOME_LATITUDE, physicsSunDeclination(longitude)), 0.05);
  }
}

static void testMoon() {
  // Phases of 2026 from USNO, UTC. 0 new, 90 first quarter, 180 full, 270 last quarter
  struct { int m, d, hh, mm; float elongation; } phases[] = {
      {1, 3, 10, 3, 180},  {1, 10, 15, 48, 270}, {1, 18, 19, 52, 0},  {1, 26, 4, 47, 90},
      {2, 1, 22, 9, 180},  {2, 9, 12, 43, 270},  {2, 17, 12, 1, 0},   {2, 24, 12, 27, 90},
      {3, 3, 11, 38, 180}, {3, 11, 9, 38, 270},  {3, 19, 1, 23, 0},   {3, 25, 19, 18, 90},
      {4, 2, 2, 12, 180},  {4, 10, 4, 51, 270},  {4, 17, 11, 52, 0},  {4, 24, 2, 32, 90}};
  for (auto& p : phases) {
    const float e = physicsMoonElongation(physicsJulianDay(2026, p.m, p.d, p.hh + p.mm / 60.0));
    float difference = fabsf(e - p.elongation);
    if (difference > 180) difference = 360 - difference;
    NEAR(difference, 0, 1.0);  // One degree: under two hours of the moon's motion
    const int expected = (int)(p.elongation / 45);
    CHECK(physicsMoonPhaseIndex(e) == expected);
    const float lit = physicsMoonIllumination(e);
    if (p.elongation == 0) CHECK(lit < 0.001f);
    if (p.elongation == 180) CHECK(lit > 0.999f);
    if (p.elongation == 90 || p.elongation == 270) NEAR(lit, 0.5, 0.02);
  }
  // Other years, from the same USNO table style: the 2024 eclipse, a new moon
  NEAR(physicsMoonIllumination(physicsMoonElongation(physicsJulianDay(2024, 4, 8, 18 + 21 / 60.0))), 0, 0.001);
  // Works from the clock as well
  const double jd = physicsJulianDayFromUnix(1775000000.0);
  CHECK(physicsMoonElongation(jd) >= 0 && physicsMoonElongation(jd) < 360);
  CHECK(isfinite(physicsMoonElongation(0)));
  CHECK(isfinite(physicsMoonElongation(1e7)));
  // Names go round in order
  CHECK(physicsMoonPhaseIndex(0) == 0);
  CHECK(physicsMoonPhaseIndex(23) == 1);
  CHECK(physicsMoonPhaseIndex(90) == 2);
  CHECK(physicsMoonPhaseIndex(180) == 4);
  CHECK(physicsMoonPhaseIndex(270) == 6);
  CHECK(physicsMoonPhaseIndex(359) == 0);
  // The drawn disc: its lit lines add up to the lit share
  for (float lit : {0.0f, 0.1f, 0.5f, 0.73f, 1.0f}) {
    for (bool growing : {true, false}) {
      float sum = 0, whole = 0;
      const float radius = 40;
      for (float y = -radius; y <= radius; y += 0.1f) {
        float from, to;
        physicsLitSpan(y, radius, lit, growing, from, to);
        CHECK(to >= from - 1e-3f);
        if (growing) CHECK(to >= 0);
        sum += to - from;
        whole += 2 * sqrtf(fmaxf(0, radius * radius - y * y));
      }
      NEAR(sum / whole, lit, 0.005);
    }
  }
  float from, to;
  physicsLitSpan(100, 40, 0.5f, true, from, to);  // Outside the disc: nothing, no crash
  NEAR(to - from, 0, 1e-5);
}

// Shells from PubChem's configurations, generated into the table
static void testAtoms() {
  CHECK(PHYSICS_ELEMENT_COUNT == 36);
  for (int i = 0; i < PHYSICS_ELEMENT_COUNT; i++) {
    const PhysicsElement& e = PHYSICS_ELEMENTS[i];
    CHECK(e.shells[0] + e.shells[1] + e.shells[2] + e.shells[3] == i + 1);  // As many electrons as protons
    CHECK(e.shells[0] <= 2 && e.shells[1] <= 8 && e.shells[2] <= 18 && e.shells[3] <= 32);  // 2 n^2
  }
  struct { int z; const char* symbol; int s[4]; } known[] = {
      {1, "H", {1, 0, 0, 0}},  {2, "He", {2, 0, 0, 0}},  {6, "C", {2, 4, 0, 0}},  {8, "O", {2, 6, 0, 0}},
      {11, "Na", {2, 8, 1, 0}}, {18, "Ar", {2, 8, 8, 0}}, {19, "K", {2, 8, 8, 1}}, {21, "Sc", {2, 8, 9, 2}},
      {24, "Cr", {2, 8, 13, 1}}, {26, "Fe", {2, 8, 14, 2}}, {29, "Cu", {2, 8, 18, 1}}, {36, "Kr", {2, 8, 18, 8}}};
  for (auto& k : known) {
    const PhysicsElement& e = PHYSICS_ELEMENTS[k.z - 1];
    CHECK(strcmp(e.symbol, k.symbol) == 0);
    for (int s = 0; s < 4; s++) CHECK(e.shells[s] == k.s[s]);
  }
}

static void testNumbers() {
  char buffer[16];
  physicsFormatNumber(buffer, sizeof(buffer), 2.5f, 1, false);
  CHECK(strcmp(buffer, "2.5") == 0);
  physicsFormatNumber(buffer, sizeof(buffer), 2.5f, 1, true);
  CHECK(strcmp(buffer, "2,5") == 0);
  physicsFormatNumber(buffer, sizeof(buffer), 9.81f, 2, true);
  CHECK(strcmp(buffer, "9,81") == 0);
  physicsFormatNumber(buffer, sizeof(buffer), 3.0f, 0, true);
  CHECK(strcmp(buffer, "3") == 0);
  physicsFormatNumber(buffer, 4, 123456.0f, 1, true);  // Too long: cut, never past the buffer
  CHECK(strlen(buffer) == 3);
}

static void testScenes() {
  CHECK(physicsFindScene("pendulum") >= 0);
  CHECK(physicsFindScene("PENDULUM") == physicsFindScene("pendulum"));
  const char* bad[] = {"", " ", "pendulu", "pendulums", "pendulum ", "pendulum;x", "../lever", "%6Cever",
                       ("Pendel"), "\xFF\xFE", "orbital-2p", "a very long name that is not on any list at all"};
  for (const char* s : bad) CHECK(physicsFindScene(s) == -1);
  CHECK(physicsFindScene(nullptr) == -1);
  std::string huge(100000, 'a');
  CHECK(physicsFindScene(huge.c_str()) == -1);
  for (int i = 0; i < PHYSICS_SCENE_COUNT; i++) {
    CHECK(PHYSICS_SCENES[i].paramCount >= 1);
    CHECK(strlen(PHYSICS_SCENES[i].id) < 32);
  }
}

// The closed list in the prompt and the table are the same, in both directions
static void testPromptRule() {
  const std::string rule = PHYSICS_PROMPT_RULE;
  for (char c : rule) CHECK((unsigned char)c < 0x80);  // Plain ASCII
  const std::string marker = "id genau eine von: ";
  const size_t start = rule.find(marker);
  CHECK(start != std::string::npos);
  const size_t end = rule.find('.', start);
  std::vector<std::string> listed;
  std::string word;
  for (size_t i = start + marker.size(); i <= end; i++) {
    if (rule[i] == ' ' || rule[i] == '.') {
      if (!word.empty()) listed.push_back(word);
      word.clear();
    } else {
      word += rule[i];
    }
  }
  CHECK((int)listed.size() == PHYSICS_SCENE_COUNT);
  for (const std::string& id : listed) CHECK(physicsFindScene(id.c_str()) >= 0);
  for (int i = 0; i < PHYSICS_SCENE_COUNT; i++) {
    bool found = false;
    for (const std::string& id : listed) found = found || id == PHYSICS_SCENES[i].id;
    CHECK(found);
  }
}

int main() {
  testScenes();
  testPromptRule();
  testNumbers();
  testFalling();
  testFriction();
  testLever();
  testPulley();
  testBuoyancy();
  testMagnet();
  testPendulum();
  testRamp();
  testShadow();
  testMirror();
  testRefraction();
  testSound();
  testCircuit();
  testMatter();
  testJulianDay();
  testSeasons();
  testMoon();
  testAtoms();
  return finish("physics");
}
