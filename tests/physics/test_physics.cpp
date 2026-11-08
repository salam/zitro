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
  // The table holds the sources' values (docs/features/physics.md)
  NEAR(PHYSICS_WORLDS[0].g, 9.81, 1e-4);
  NEAR(PHYSICS_WORLDS[1].g, 1.62, 1e-4);
  NEAR(PHYSICS_WORLDS[2].g, 3.73, 1e-4);
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

// Pressure is -dxi/dx: checked against the displacement itself, by small differences
static void testSoundPressure() {
  const float wavelength = 94, period = 1, amplitude = 5;
  const float k = 2 * PHYSICS_PI / wavelength;
  for (float x = 0; x < 200; x += 7) {
    for (float t : {0.0f, 0.13f, 0.6f}) {
      const float h = 0.5f;
      const float slope = (physicsWaveDisplacement(amplitude, wavelength, period, x + h, t) -
                           physicsWaveDisplacement(amplitude, wavelength, period, x - h, t)) / (2 * h);
      NEAR(physicsWavePressure(amplitude * k, wavelength, period, x, t), -slope, 0.002);
    }
  }
  // Air pushed together from both sides, the highest pressure: left of it pushed right, right of it pushed left
  float best = -1e9f, bestX = 0;
  for (float x = 0; x < wavelength; x += 0.25f) {
    const float p = physicsWavePressure(1, wavelength, period, x, 0);
    if (p > best) { best = p; bestX = x; }
  }
  CHECK(physicsWaveDisplacement(amplitude, wavelength, period, bestX - wavelength / 8, 0) > 0);
  CHECK(physicsWaveDisplacement(amplitude, wavelength, period, bestX + wavelength / 8, 0) < 0);
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
  // A liquid lies on the floor over time: never a blob hanging in the air. Sampled for a while.
  static PhysicsMatter m;
  physicsMatterInit(m);
  for (int i = 0; i < 6000; i++) physicsMatterStep(m, PHYSICS_MATTER_LIQUID_T);
  float highestParticle = 0, lowestParticleMax = 0, widest = 0;
  for (int i = 0; i < 12000; i++) {
    physicsMatterStep(m, PHYSICS_MATTER_LIQUID_T);
    if (i % 100) continue;
    float low = 99, high = 0, left = 99, right = 0;
    for (int k = 0; k < PHYSICS_MATTER_COUNT; k++) {
      low = fminf(low, m.y[k]);
      high = fmaxf(high, m.y[k]);
      left = fminf(left, m.x[k]);
      right = fmaxf(right, m.x[k]);
    }
    highestParticle = fmaxf(highestParticle, high);
    lowestParticleMax = fmaxf(lowestParticleMax, low);
    widest = fmaxf(widest, right - left);
  }
  printf("liquid: lowest particle stays under %.2f, highest %.2f, widest %.2f\n", lowestParticleMax, highestParticle, widest);
  CHECK(lowestParticleMax < 1.2f);
  CHECK(highestParticle < 0.9f * PHYSICS_MATTER_HEIGHT);
}

// Heated to a gas and cooled again, the sample comes back down and stops wandering
static void testMatterRecools() {
  static PhysicsMatter m;
  physicsMatterInit(m);
  for (int i = 0; i < 6000; i++) physicsMatterStep(m, PHYSICS_MATTER_GAS_T);
  for (int i = 0; i < 40000; i++) physicsMatterStep(m, PHYSICS_MATTER_SOLID_T);
  float x0[PHYSICS_MATTER_COUNT], y0[PHYSICS_MATTER_COUNT];
  memcpy(x0, m.x, sizeof(x0));
  memcpy(y0, m.y, sizeof(y0));
  for (int i = 0; i < 4000; i++) physicsMatterStep(m, PHYSICS_MATTER_SOLID_T);
  float wander = 0;
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) wander += hypotf(m.x[i] - x0[i], m.y[i] - y0[i]) / PHYSICS_MATTER_COUNT;
  printf("matter recooled: wander %.2f height %.2f\n", wander, matterMeanHeight(m));
  CHECK(wander < 0.4f);
  CHECK(matterMeanHeight(m) < 2.6f);
}

static void testClock() {
  double day = 0;
  CHECK(!physicsDayFromClock(0, day));  // Not set yet: no "today"
  NEAR(day, PHYSICS_REFERENCE_NEW_MOON, 1e-6);
  NEAR(physicsMoonIllumination(physicsMoonElongation(day)), 0, 0.002);  // And it really is a new moon
  CHECK(!physicsDayFromClock(1000000.0, day));
  CHECK(!physicsDayFromClock(-5.0, day));
  CHECK(!physicsDayFromClock(NAN, day));
  CHECK(physicsDayFromClock(1775000000.0, day));
  NEAR(day, physicsJulianDayFromUnix(1775000000.0), 1e-6);
  // Button B through the year comes back around, one season at a time
  float longitude = 100 * PHYSICS_DEG;
  const int order[4] = {2, 3, 0, 1};
  for (int i = 0; i < 8; i++) {
    longitude = physicsNextSeasonLongitude(longitude);
    CHECK(physicsSeasonIndex(longitude) == order[i % 4]);
  }
  CHECK(physicsSeasonIndex(physicsNextSeasonLongitude(359.9f * PHYSICS_DEG)) == 0);
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
  for (const std::string& id : listed) {
    if (id == "atom-<Symbol>") continue;
    int scene, element;
    CHECK(physicsParseQuery(id.c_str(), scene, element));
  }
  for (int i = 0; i < PHYSICS_SCENE_COUNT; i++) {
    const std::string wanted = PHYSICS_SCENES[i].scene == PHYSICS_ATOM ? "atom-<Symbol>" : PHYSICS_SCENES[i].id;
    bool found = false;
    for (const std::string& id : listed) found = found || id == wanted;
    CHECK(found);
  }
}

// Atoms must not go to the molecule rule, which comes first in the prompt
static void testPromptKeepsAtomsFromMolecule() {
  const std::string rule = PHYSICS_PROMPT_RULE;
  CHECK(rule.find("physics:atom-<Symbol>") != std::string::npos);
  CHECK(rule.find("nie molecule") != std::string::npos);
}

static void testAtomQueries() {
  int scene, element;
  CHECK(physicsParseQuery("atom-C", scene, element));
  CHECK(element == 6);
  CHECK(PHYSICS_SCENES[scene].scene == PHYSICS_ATOM);
  CHECK(physicsParseQuery("atom-Fe", scene, element) && element == 26);
  CHECK(physicsParseQuery("atom-fe", scene, element) && element == 26);
  CHECK(physicsParseQuery("ATOM-KR", scene, element) && element == 36);
  CHECK(physicsParseQuery("atom-iron", scene, element) && element == 26);
  CHECK(physicsParseQuery("atom-H", scene, element) && element == 1);
  // Symbols are matched whole: Co is cobalt, C is carbon, never a mix
  CHECK(physicsParseQuery("atom-Co", scene, element) && element == 27);
  // Lookup keeps working for the other scenes, with no element
  CHECK(physicsParseQuery("lever", scene, element) && element == 0);
  CHECK(PHYSICS_SCENES[scene].scene == PHYSICS_LEVER);
  const char* bad[] = {"atom", "atom-", "atom-U", "atom-Rb", "atom-uranium", "atom-Xx", "atom-C ", "atom-C;",
                       "atom--C", "ato-C", "", "unknown", "atom-\xFF", "atom-kohlenstoff"};
  for (const char* q : bad) {
    CHECK(!physicsParseQuery(q, scene, element));
    CHECK(scene == -1);
  }
  CHECK(!physicsParseQuery(nullptr, scene, element));
  std::string huge = "atom-" + std::string(100000, 'a');
  CHECK(!physicsParseQuery(huge.c_str(), scene, element));
}

// ---- New scenes ----
static void testDoublePendulum() {
  const float m = 1, l = PHYSICS_CHAOS_LENGTH, g = 9.81f;
  // Hanging still stays still
  PhysicsDoublePendulum rest = {0, 0, 0, 0};
  float a1, a2;
  physicsDoublePendulumAccel(rest, m, m, l, l, g, a1, a2);
  NEAR(a1, 0, 1e-6);
  NEAR(a2, 0, 1e-6);
  // Released level with the pivot, rods in line: the first weight starts with g / l * ... check by hand.
  // At angle1 = angle2 = 90 degrees and at rest: den = 2 m1 + m2 - m2 cos 0 = 2; a1 = -g (3) / (l 2) + m2 g sin(90-180)...
  PhysicsDoublePendulum level = {PHYSICS_PI / 2, 0, PHYSICS_PI / 2, 0};
  physicsDoublePendulumAccel(level, m, m, l, l, g, a1, a2);
  // -g(2m1+m2) sin(a1) - m2 g sin(a1 - 2 a2) = -3g - g sin(-90 deg) = -2g; over l (2m1 + m2 - m2) = 2 l
  NEAR(a1, -g / l, 1e-4);
  NEAR(a2, 0, 1e-4);
  // Small swings: the slow normal mode has omega^2 = (2 - sqrt 2) g / l for equal masses and rods
  PhysicsDoublePendulum mode = {0.01f, 0, 0.01f * sqrtf(2.0f), 0};
  physicsDoublePendulumAccel(mode, m, m, l, l, g, a1, a2);
  NEAR(a1 / mode.angle1, -(2 - sqrtf(2)) * g / l, 0.01 * g / l);
  NEAR(a2 / mode.angle2, -(2 - sqrtf(2)) * g / l, 0.01 * g / l);
  // Energy stays, for every start the scene uses, over 30 s at the scene's step
  for (float start : PHYSICS_CHAOS_STARTS) {
    PhysicsDoublePendulum p = {start * PHYSICS_DEG, 0, start * PHYSICS_DEG, 0};
    const float e0 = physicsDoublePendulumEnergy(p, m, m, l, l, g);
    float worst = 0;
    for (int i = 0; i < 15000; i++) {
      physicsDoublePendulumStep(p, m, m, l, l, g, 0.002f);
      worst = fmaxf(worst, fabsf(physicsDoublePendulumEnergy(p, m, m, l, l, g) - e0));
    }
    // Against the energy scale m g l: a tenth of a percent
    NEAR(worst / (m * g * l), 0, 1e-3);
  }
  // Chaos: from 135 degrees a thousandth of a radian grows to a different motion within 30 s;
  // from 30 degrees it stays small
  for (float start : {30.0f, 135.0f}) {
    PhysicsDoublePendulum p = {start * PHYSICS_DEG, 0, start * PHYSICS_DEG, 0};
    PhysicsDoublePendulum q = {start * PHYSICS_DEG + PHYSICS_CHAOS_NUDGE, 0, start * PHYSICS_DEG, 0};
    float gap = 0;
    for (int i = 0; i < 15000; i++) {
      physicsDoublePendulumStep(p, m, m, l, l, g, 0.002f);
      physicsDoublePendulumStep(q, m, m, l, l, g, 0.002f);
      const float px = l * (sinf(p.angle1) + sinf(p.angle2)), py = -l * (cosf(p.angle1) + cosf(p.angle2));
      const float qx = l * (sinf(q.angle1) + sinf(q.angle2)), qy = -l * (cosf(q.angle1) + cosf(q.angle2));
      gap = fmaxf(gap, hypotf(px - qx, py - qy));
    }
    printf("double pendulum from %.0f deg: largest gap %.3f m\n", start, gap);
    if (start < 45) CHECK(gap < 0.01f);
    else CHECK(gap > 0.1f);
  }
}

static void testDoppler() {
  const float c = 343.0f;
  NEAR(physicsDopplerHeard(440, c, 0, 1), 440, 1e-3);
  NEAR(physicsDopplerHeard(440, c, 27.78f, 1), 440 / (1 - 27.78 / 343.0), 0.01);   // Coming straight on
  NEAR(physicsDopplerHeard(440, c, 27.78f, -1), 440 / (1 + 27.78 / 343.0), 0.01);  // Going away
  NEAR(physicsDopplerHeard(440, c, 27.78f, 0), 440, 1e-3);                         // Passing right by
  // The pitch is the rate at which crests arrive: crests sent every 1 / 440 s, timed at the listener
  for (float kmh : PHYSICS_DOPPLER_KMH) {
    const float v = kmh / 3.6f;
    for (float sent : {0.0f, 2.0f, 4.0f, 6.0f}) {
      float cos0, cos1;
      const double t0 = physicsDopplerArrival(sent, -100, v, c, 0, 30, cos0);
      const double t1 = physicsDopplerArrival(sent + 1.0f / 440, -100, v, c, 0, 30, cos1);
      NEAR(1.0 / (t1 - t0), physicsDopplerHeard(440, c, v, cos0), 0.5);
    }
  }
  // The screen keeps the true ratio: the same formula in pixels gives the same pitch
  float cosScreen;
  physicsDopplerArrival(1.0f, -50, 300 / 3.6f / c * PHYSICS_DOPPLER_SCREEN_C, PHYSICS_DOPPLER_SCREEN_C, 120, 56, cosScreen);
  NEAR(physicsDopplerHeard(440, PHYSICS_DOPPLER_SCREEN_C, 300 / 3.6f / c * PHYSICS_DOPPLER_SCREEN_C, cosScreen),
       physicsDopplerHeard(440, c, 300 / 3.6f, cosScreen), 0.01);
}

static void testDoubleSlit() {
  NEAR(physicsTwoWaveBrightness(100, 100, 8), 4, 1e-5);   // In the middle: both crests together
  NEAR(physicsTwoWaveBrightness(104, 100, 8), 0, 1e-5);   // Half a wave apart: dark
  NEAR(physicsTwoWaveBrightness(108, 100, 8), 4, 1e-4);   // A whole wave: bright again
  NEAR(physicsTwoWaveBrightness(102, 100, 8), 2, 1e-4);
  // Far away the bright bands lie lambda L / d apart
  const float d = 40, L = 4000, lambda = 8;
  float firstDark = 0, darkest = 5;
  for (float y = 300; y < 500; y += 0.05f) {
    const float r1 = hypotf(L, y - d / 2), r2 = hypotf(L, y + d / 2);
    const float b = physicsTwoWaveBrightness(r1, r2, lambda);
    if (b < darkest) { darkest = b; firstDark = y; }
  }
  // Path difference d sin(a) = lambda / 2, at y = L tan(a)
  NEAR(firstDark, L * tanf(asinf(lambda / (2 * d))), 0.5);
  NEAR(firstDark, lambda * L / d / 2, 3.0);  // The small-angle rule
  CHECK(PHYSICS_LASERS[0].nanometres > PHYSICS_LASERS[1].nanometres);
  CHECK(PHYSICS_LASERS[1].nanometres > PHYSICS_LASERS[2].nanometres);
}

static void testPrism() {
  const PhysicsGlass& water = PHYSICS_GLASSES[0];
  const PhysicsGlass& crown = PHYSICS_GLASSES[1];
  const PhysicsGlass& flint = PHYSICS_GLASSES[2];
  // SCHOTT's catalogue: nd at 587.56 nm, nF at 486.13 nm, nC at 656.27 nm
  NEAR(physicsSellmeier(crown, 0.58756f), 1.51680, 2e-5);
  NEAR(physicsSellmeier(crown, 0.48613f), 1.52238, 2e-5);
  NEAR(physicsSellmeier(crown, 0.65627f), 1.51432, 2e-5);
  NEAR(physicsSellmeier(flint, 0.58756f), 1.78472, 2e-5);
  // Abbe numbers in the catalogue: N-BK7 64.17, N-SF11 25.68
  const float vCrown = (physicsSellmeier(crown, 0.58756f) - 1) / (physicsSellmeier(crown, 0.48613f) - physicsSellmeier(crown, 0.65627f));
  const float vFlint = (physicsSellmeier(flint, 0.58756f) - 1) / (physicsSellmeier(flint, 0.48613f) - physicsSellmeier(flint, 0.65627f));
  NEAR(vCrown, 64.17, 0.05);
  NEAR(vFlint, 25.68, 0.05);
  // Water at 20 C, 589.3 nm: handbooks give 1.3330; Daimon and Masumura's fit lies 0.0004 above
  NEAR(physicsSellmeier(water, 0.5893f), 1.3330, 5e-4);
  // Violet bends more than red
  for (const PhysicsGlass& g : PHYSICS_GLASSES) CHECK(physicsSellmeier(g, 0.41f) > physicsSellmeier(g, 0.66f));
  // Snell in vector form agrees with the angle form
  for (float a : {0.0f, 20.0f, 45.0f, 70.0f}) {
    const float in = a * PHYSICS_DEG;
    float ox, oy, out;
    CHECK(physicsRefract(sinf(in), cosf(in), 0, -1, 1.0f, 1.5f, ox, oy));
    CHECK(physicsSnell(1.0f, 1.5f, in, out));
    NEAR(atan2f(ox, oy), out, 1e-5);
    NEAR(hypotf(ox, oy), 1, 1e-5);
  }
  float ox, oy;
  CHECK(!physicsRefract(sinf(60 * PHYSICS_DEG), cosf(60 * PHYSICS_DEG), 0, -1, 1.5f, 1.0f, ox, oy));  // Total reflection
  // A 60-degree prism: minimum deviation 2 asin(n / 2) - 60 degrees when the ray goes through evenly
  const float n = 1.5168f;
  const float even = asinf(n * sinf(30 * PHYSICS_DEG));
  NEAR(physicsPrismDeviation(60 * PHYSICS_DEG, even, n) / PHYSICS_DEG, 2 * even / PHYSICS_DEG - 60, 1e-3);
  NEAR(physicsPrismDeviation(60 * PHYSICS_DEG, even, n) / PHYSICS_DEG, 38.647, 0.01);  // By hand: 2 x 49.3233 - 60
  // The traced ray through the drawn prism turns by exactly the textbook deviation
  const float h = 60 * sqrtf(3) / 2;
  const float x[3] = {100, 70, 130}, y[3] = {20, 20 + h, 20 + h};  // Apex up, screen y down
  for (const PhysicsGlass& g : PHYSICS_GLASSES) {
    for (float nm : PHYSICS_PRISM_NM) {
      const float index = physicsSellmeier(g, nm / 1000.0f);
      // In at 60 degrees from the left face's normal, rising 30 degrees, at the face's middle
      const float dx = cosf(30 * PHYSICS_DEG), dy = -sinf(30 * PHYSICS_DEG);
      const float mx = 85, my = 20 + h / 2;
      float inX, inY, outX, outY, outDx, outDy;
      const bool through = physicsPrismRay(x, y, index, mx - 50 * dx, my - 50 * dy, dx, dy, inX, inY, outX, outY, outDx, outDy);
      const float expected = physicsPrismDeviation(60 * PHYSICS_DEG, 60 * PHYSICS_DEG, index);
      CHECK(through == (expected >= 0));
      if (!through) continue;
      NEAR(inX, mx, 1e-2);
      NEAR(inY, my, 1e-2);
      const float turned = atan2f(-dy, dx) - atan2f(-outDy, outDx);
      NEAR(turned, expected, 1e-4);
    }
  }
  // Flint spreads the colours several times wider than crown glass
  const float spreadCrown = physicsPrismDeviation(60 * PHYSICS_DEG, 60 * PHYSICS_DEG, physicsSellmeier(crown, 0.41f)) -
                            physicsPrismDeviation(60 * PHYSICS_DEG, 60 * PHYSICS_DEG, physicsSellmeier(crown, 0.66f));
  const float spreadFlint = physicsPrismDeviation(60 * PHYSICS_DEG, 60 * PHYSICS_DEG, physicsSellmeier(flint, 0.41f)) -
                            physicsPrismDeviation(60 * PHYSICS_DEG, 60 * PHYSICS_DEG, physicsSellmeier(flint, 0.66f));
  printf("prism spread at 60 deg in: crown %.2f deg, flint %.2f deg\n", spreadCrown / PHYSICS_DEG, spreadFlint / PHYSICS_DEG);
  CHECK(spreadFlint > 4 * spreadCrown);
}

static void testHalfLife() {
  NEAR(physicsDecayChance(1, 1), 0.5, 1e-6);
  NEAR(physicsDecayChance(2, 1), 0.75, 1e-6);
  NEAR(physicsDecayChance(0, 5), 0, 1e-7);
  // Many small steps make the same as one big one
  NEAR(powf(1 - physicsDecayChance(0.04f, 6), 150), 0.5, 1e-4);
  NEAR(physicsRemaining(256, 12, 6), 64, 1e-3);
  // Values as fetched from the IAEA (ENSDF): seconds in the database
  NEAR(PHYSICS_ISOTOPES[0].halfLife * 60, 153.12, 1e-2);
  NEAR(PHYSICS_ISOTOPES[1].halfLife * 86400, 330238.08, 1);
  NEAR(PHYSICS_ISOTOPES[2].halfLife * 86400, 693377.28, 1);
  NEAR(PHYSICS_ISOTOPES[3].halfLife, 5700, 1e-3);
  // The chance at work: of 100,000 atoms about half are left after one half-life, in frames of 0.04 s
  uint32_t seed = 12345;
  int alive = 100000;
  const float p = physicsDecayChance(0.04f, 6);
  for (int frame = 0; frame < 150; frame++) {
    int decayed = 0;
    for (int i = 0; i < alive; i++) decayed += physicsRandom(seed) < p;
    alive -= decayed;
  }
  NEAR(alive, 50000, 600);  // Three standard deviations of a coin toss
  // The random numbers stay in 0..1 and look even
  double sum = 0;
  for (int i = 0; i < 100000; i++) {
    const float r = physicsRandom(seed);
    CHECK(r >= 0 && r < 1);
    sum += r;
  }
  NEAR(sum / 100000, 0.5, 0.005);
  uint32_t zero = 0;
  physicsRandom(zero);
  CHECK(zero != 0);  // Never stuck at zero
}

// Heliocentric positions from JPL Horizons (ecliptic J2000, au), fetched 2026-11-08; tests only
static void testPlanets() {
  struct { int planet; double jd, x, y, z, arcsec, km; } known[] = {
      {ASTRO_MERCURY, 2461352.5, 1.426679889821202E-01, 2.742329219529482E-01, 9.326496131142224E-03, 15, 1000},
      {ASTRO_VENUS, 2461352.5, 4.228501738023033E-01, 5.852965432546771E-01, -1.635474321913021E-02, 20, 4000},
      {ASTRO_EARTH, 2461352.5, 6.980065523764718E-01, 7.033680315656204E-01, -4.761108307144484E-05, 20, 6000},
      {ASTRO_MARS, 2461352.5, -3.803548960049689E-01, 1.555361863138156E+00, 4.192095066905015E-02, 40, 25000},
      {ASTRO_JUPITER, 2461352.5, -3.704869589854907E+00, 3.816084275360042E+00, 6.703955657933161E-02, 400, 600000},
      {ASTRO_SATURN, 2461352.5, 9.205312882435891E+00, 1.973932130985622E+00, -4.008050245906088E-01, 600, 1500000},
      {ASTRO_URANUS, 2461352.5, 8.796393679089622E+00, 1.732963178364433E+01, -4.969413484915876E-02, 50, 1000000},
      {ASTRO_NEPTUNE, 2461352.5, 2.983082737434598E+01, 1.499908409751931E+00, -7.183325823754320E-01, 10, 200000},
      {ASTRO_MARS, 2451545.0, 1.390715921746351E+00, -1.341631815101244E-02, -3.446766277581799E-02, 40, 25000},
      {ASTRO_EARTH, 2451545.0, -1.771587841839055E-01, 9.672193524609504E-01, -1.139275508446145E-06, 20, 6000},
      {ASTRO_MARS, 2433282.5, -1.395553774894103E+00, 9.043964307251104E-01, 5.337868315157205E-02, 40, 25000},
      {ASTRO_EARTH, 2433282.5, -1.827019777847186E-01, 9.661458551954407E-01, 1.101819495711953E-04, 20, 6000},
  };
  for (auto& k : known) {
    double p[3];
    astroPlanetPosition(k.planet, k.jd, p);
    const double r = sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    const double rk = sqrt(k.x * k.x + k.y * k.y + k.z * k.z);
    // Within four times JPL's nominal errors for 1800-2050 (they are typical errors, not limits:
    // Neptune in 2026 is off by 32 arcseconds against a nominal 10). Far below a pixel on screen.
    const double lon = astroWrap180(atan2(p[1], p[0]) / ASTRO_RAD - atan2(k.y, k.x) / ASTRO_RAD) * 3600;
    NEAR(lon, 0, 4 * k.arcsec);
    NEAR((r - rk) * ASTRO_AU_KM, 0, 4 * k.km);
    const double lat = (asin(p[2] / r) - asin(k.z / rk)) / ASTRO_RAD * 3600;
    NEAR(lat, 0, 60);
  }
  // Kepler's equation is solved
  for (double e : {0.0, 0.0167, 0.2056, 0.9}) {
    for (double M = -3.1; M < 3.15; M += 0.3) {
      const double E = astroEccentricAnomaly(M, e);
      NEAR(E - e * sin(E), M, 1e-10);
    }
  }
  // Mars oppositions, from Horizons' geocentric longitudes: 2020-10-13 23 UT and 2027-02-19 to 20
  const double opposition2020 = astroNextOpposition(ASTRO_MARS, physicsJulianDay(2020, 6, 1, 0));
  NEAR(opposition2020, physicsJulianDay(2020, 10, 13, 23), 1.0);
  const double opposition2027 = astroNextOpposition(ASTRO_MARS, physicsJulianDay(2026, 11, 8, 0));
  NEAR(opposition2027, physicsJulianDay(2027, 2, 19, 12), 1.5);
  // Jupiter's opposition of 2026: 2026-01-10
  NEAR(astroNextOpposition(ASTRO_JUPITER, physicsJulianDay(2025, 6, 1, 0)), physicsJulianDay(2026, 1, 10, 0), 1.5);
  // Mars ran backwards from about 2020-09-09 to 2020-11-14 (Horizons); with precession taken out,
  // its longitude on 2020-10-13 is Horizons' 21.390 degrees of date minus 0.290 degrees
  double l1, b1, l2, b2;
  astroGeocentric(ASTRO_MARS, physicsJulianDay(2020, 10, 13, 0), l1, b1);
  NEAR(l1, 21.390 - 0.290, 0.05);
  NEAR(b1, -3.058, 0.05);
  astroGeocentric(ASTRO_MARS, physicsJulianDay(2020, 10, 14, 0), l2, b2);
  CHECK(l2 < l1);  // Backwards
  astroGeocentric(ASTRO_MARS, physicsJulianDay(2020, 8, 20, 0), l1, b1);
  astroGeocentric(ASTRO_MARS, physicsJulianDay(2020, 8, 21, 0), l2, b2);
  CHECK(l2 > l1);  // Forwards again before
  astroGeocentric(ASTRO_MARS, physicsJulianDay(2020, 12, 1, 0), l1, b1);
  astroGeocentric(ASTRO_MARS, physicsJulianDay(2020, 12, 2, 0), l2, b2);
  CHECK(l2 > l1);  // And after
  // Calendar from a Julian day, Meeus's examples
  int y, m, d;
  astroCalendarDate(2436116.31, y, m, d);
  CHECK(y == 1957 && m == 10 && d == 4);
  astroCalendarDate(2451545.0, y, m, d);
  CHECK(y == 2000 && m == 1 && d == 1);
  astroCalendarDate(physicsJulianDay(2026, 3, 1, 0), y, m, d);
  CHECK(y == 2026 && m == 3 && d == 1);
  astroCalendarDate(physicsJulianDay(2024, 2, 29, 23.9), y, m, d);
  CHECK(y == 2024 && m == 2 && d == 29);
  CHECK(ASTRO_ELEMENTS_FROM == physicsJulianDay(1800, 1, 1, 0));
  CHECK(ASTRO_ELEMENTS_TO == physicsJulianDay(2050, 1, 1, 0));
}

static void testSunInTheSky() {
  // Sun's direction and height from JPL Horizons, no air (refraction), 8.5 E 47 N and 151.2 E 33.9 S
  struct { double jd; float lat, lon, azimuth, height; } known[] = {
      {2461120.75, 47, 8.5, 94.736794f, 4.743184f},
      {2461212.916666667, 47, 8.5, 135.672926f, 60.605391f},
      {2461352.375, 47, 8.5, 307.785469f, -49.414083f},
      {2461395.083333333, 47, 8.5, 216.152427f, 11.231911f},
      {2461212.916666667, -33.9f, 151.2f, 274.964843f, -37.679309f},
      {2461395.083333333, -33.9f, 151.2f, 178.041867f, -32.645631f},
  };
  for (auto& k : known) {
    float height, azimuth;
    physicsSunNow(k.jd, k.lat * PHYSICS_DEG, k.lon * PHYSICS_DEG, height, azimuth);
    NEAR(height / PHYSICS_DEG, k.height, 0.1);
    NEAR(azimuth / PHYSICS_DEG, k.azimuth, 0.15);
  }
  // At noon the Sun is due south, at the height the seasons scene gives
  float height, azimuth;
  physicsSkyPosition(PHYSICS_HOME_LATITUDE, PHYSICS_EARTH_TILT, 0, height, azimuth);
  NEAR(azimuth / PHYSICS_DEG, 180, 1e-3);
  NEAR(height, physicsNoonHeight(PHYSICS_HOME_LATITUDE, PHYSICS_EARTH_TILT), 1e-4);
  // South of the equator it is due north at noon
  physicsSkyPosition(-33.9f * PHYSICS_DEG, 0, 0, height, azimuth);
  CHECK(azimuth / PHYSICS_DEG < 1e-3 || azimuth / PHYSICS_DEG > 360 - 1e-3);
  // At the equinox it rises due east and sets due west, after 12 hours
  const float set = acosf(-tanf(PHYSICS_HOME_LATITUDE) * tanf(0));
  physicsSkyPosition(PHYSICS_HOME_LATITUDE, 0, -set, height, azimuth);
  NEAR(height, 0, 1e-5);
  NEAR(azimuth / PHYSICS_DEG, 90, 1e-3);
  physicsSkyPosition(PHYSICS_HOME_LATITUDE, 0, set, height, azimuth);
  NEAR(azimuth / PHYSICS_DEG, 270, 1e-3);
  // Sidereal time, Meeus's example 12.a: 1987-04-10 0 UT, 13h 10m 46.3668s
  NEAR(physicsSiderealDegrees(2446895.5), (13 + 10 / 60.0 + 46.3668 / 3600.0) * 15, 1e-3);
}

static void testSatellites() {
  // Kepler's third law: the ISS goes round in about an hour and a half
  NEAR(astroOrbitPeriodOf(ASTRO_ORBITS[0]) / 60, 92.9, 0.1);
  // GPS: half a sidereal day (11 h 58 min), give or take its rounded height
  NEAR(astroOrbitPeriodOf(ASTRO_ORBITS[1]) / 3600, PHYSICS_SIDEREAL_DAY / 2 / 3600, 0.02);
  // Geostationary: one sidereal day, 23 h 56 min 4 s, within a few seconds
  NEAR(astroOrbitPeriodOf(ASTRO_ORBITS[2]), PHYSICS_SIDEREAL_DAY, 5);
  // The Moon: NASA's 27.3217 days; Kepler with both masses gives it to a fraction of a percent
  NEAR(astroOrbitPeriodOf(ASTRO_ORBITS[3]) / 86400, 27.3217, 0.1);
  // By hand: a = 7000 km, T = 2 pi sqrt(7000^3 / 398600.4418) = 5828.5 s
  NEAR(astroOrbitPeriod(7000, ASTRO_GM_EARTH), 5828.52, 0.05);
}

static void testSizes() {
  // NASA's fact sheets give these ratios to Earth: Mercury 0.383, Moon 0.2727, Jupiter 10.973, Sun 109.2
  NEAR(ASTRO_BODIES[ASTRO_BODY_MERCURY].radiusKm / ASTRO_BODIES[ASTRO_BODY_EARTH].radiusKm, 0.383, 5e-4);
  NEAR(ASTRO_BODIES[ASTRO_BODY_MOON].radiusKm / ASTRO_BODIES[ASTRO_BODY_EARTH].radiusKm, 0.2727, 5e-4);
  NEAR(ASTRO_BODIES[ASTRO_BODY_JUPITER].radiusKm / ASTRO_BODIES[ASTRO_BODY_EARTH].radiusKm, 10.973, 5e-3);
  NEAR(ASTRO_BODIES[ASTRO_BODY_SUN].radiusKm / ASTRO_BODIES[ASTRO_BODY_EARTH].radiusKm, 109.2, 0.05);
  NEAR(ASTRO_BODIES[ASTRO_BODY_SATURN].radiusKm / ASTRO_BODIES[ASTRO_BODY_EARTH].radiusKm, 9.140, 5e-3);
  NEAR(ASTRO_BODIES[ASTRO_BODY_URANUS].radiusKm / ASTRO_BODIES[ASTRO_BODY_EARTH].radiusKm, 3.981, 5e-3);
  NEAR(ASTRO_BODIES[ASTRO_BODY_NEPTUNE].radiusKm / ASTRO_BODIES[ASTRO_BODY_EARTH].radiusKm, 3.865, 5e-3);
  NEAR(ASTRO_BODIES[ASTRO_BODY_MARS].radiusKm / ASTRO_BODIES[ASTRO_BODY_EARTH].radiusKm, 0.532, 5e-4);
  NEAR(ASTRO_BODIES[ASTRO_BODY_VENUS].radiusKm / ASTRO_BODIES[ASTRO_BODY_EARTH].radiusKm, 0.950, 5e-4);
}

int main() {
  testScenes();
  testPromptRule();
  testAtomQueries();
  testPromptKeepsAtomsFromMolecule();
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
  testSoundPressure();
  testCircuit();
  testMatter();
  testMatterRecools();
  testClock();
  testJulianDay();
  testSeasons();
  testMoon();
  testAtoms();
  testDoublePendulum();
  testDoppler();
  testDoubleSlit();
  testPrism();
  testHalfLife();
  testPlanets();
  testSunInTheSky();
  testSatellites();
  testSizes();
  return finish("physics");
}
