// School physics from textbook equations: every moving thing in the scenes comes from here.
// Plain C++ with no Arduino includes, tested on the host: tests/physics/run.sh
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "physics_atom_table.h"

static const float PHYSICS_PI = 3.14159265358979f;
static const float PHYSICS_DEG = PHYSICS_PI / 180.0f;
static const float PHYSICS_G_EARTH = 9.81f;  // m/s2

inline float physicsClamp(float v, float low, float high) {
  return v < low ? low : (v > high ? high : v);
}

// "2.5" or, in German, "2,5"
inline void physicsFormatNumber(char* out, size_t size, float value, int decimals, bool german) {
  snprintf(out, size, "%.*f", decimals, (double)value);
  if (german) {
    for (char* p = out; *p; p++) {
      if (*p == '.') *p = ',';
    }
  }
}

// ---- Falling: s = g t^2 / 2, v = g t ----
inline float physicsFallDistance(float g, float t) { return 0.5f * g * t * t; }
inline float physicsFallSpeed(float g, float t) { return g * t; }
inline float physicsFallTime(float g, float height) { return sqrtf(2.0f * height / g); }

// ---- Friction: a block slows down by mu * g, whatever it weighs ----
inline float physicsStopDistance(float speed, float mu, float g) { return speed * speed / (2.0f * mu * g); }
inline float physicsStopTime(float speed, float mu, float g) { return speed / (mu * g); }
inline float physicsSlideDistance(float speed, float mu, float g, float t) {
  const float stop = physicsStopTime(speed, mu, g);
  if (t >= stop) return physicsStopDistance(speed, mu, g);
  return speed * t - 0.5f * mu * g * t * t;
}

// ---- Lever: it balances when mass times distance is equal on both sides ----
// -1: left side goes down, 0: balanced, 1: right side goes down
inline int physicsLeverSide(float leftMass, float leftDist, float rightMass, float rightDist) {
  const float difference = rightMass * rightDist - leftMass * leftDist;
  if (fabsf(difference) < 1e-4f) return 0;
  return difference > 0 ? 1 : -1;
}

static const float PHYSICS_LEVER_MAX_ANGLE = 12.0f * PHYSICS_DEG;

// angle > 0: right side down. A classroom lever, 80 cm long, distances counted in marks 10 cm apart.
// Torques from both weights, plus the beam: its centre of mass hangs a little below the pivot,
// which is what brings a balanced lever back to level. Stops at the ground.
inline void physicsLeverStep(float& angle, float& omega, float leftMass, float leftMarks, float rightMass,
                             float rightMarks, float g, float dt) {
  const float mark = 0.1f, beamMass = 1.0f, beamLength = 0.8f, beamDrop = 0.05f, damping = 1.8f;
  const float leftDist = leftMarks * mark, rightDist = rightMarks * mark;
  const float inertia = leftMass * leftDist * leftDist + rightMass * rightDist * rightDist +
                        beamMass * beamLength * beamLength / 12.0f;
  const float torque = g * cosf(angle) * (rightMass * rightDist - leftMass * leftDist) -
                       beamMass * g * beamDrop * sinf(angle);
  omega += (torque / inertia - damping * omega) * dt;
  angle += omega * dt;
  if (angle > PHYSICS_LEVER_MAX_ANGLE) { angle = PHYSICS_LEVER_MAX_ANGLE; omega = 0; }
  if (angle < -PHYSICS_LEVER_MAX_ANGLE) { angle = -PHYSICS_LEVER_MAX_ANGLE; omega = 0; }
}

// ---- Pulley: n ropes share the load, and you pull n times as much rope ----
inline float physicsPulleyForce(float weight, int ropes) { return ropes > 0 ? weight / ropes : weight; }
inline float physicsPulleyRope(float lift, int ropes) { return lift * ropes; }

// ---- Buoyancy: Archimedes. It floats when lighter than the water it pushes away ----
static const float PHYSICS_WATER_DENSITY = 1.0f;  // g/cm3
static const float PHYSICS_CUBE_SIDE = 0.1f;      // m

inline bool physicsFloats(float density, float fluidDensity) { return density < fluidDensity; }
// Part of a floating body below the surface
inline float physicsFloatFraction(float density, float fluidDensity) {
  return physicsClamp(density / fluidDensity, 0.0f, 1.0f);
}

// A cube dropped into water. depth: how far its underside is below the surface (m, negative above
// it). Weight, buoyancy, water drag and the water it has to push along. floorDepth: bottom of the tank.
inline void physicsBuoyancyStep(float& depth, float& speed, float density, float floorDepth, float g, float dt) {
  const float side = PHYSICS_CUBE_SIDE;
  const float rho = density * 1000.0f, rhoWater = PHYSICS_WATER_DENSITY * 1000.0f;
  const float area = side * side;
  const float under = physicsClamp(depth, 0.0f, side);
  const float mass = rho * area * side;
  const float addedMass = 0.5f * rhoWater * area * under;
  float force = mass * g - rhoWater * area * under * g;
  if (under > 0) {
    const float drag = 0.5f * 1.05f * rhoWater * area * speed * fabsf(speed);  // Drag coefficient of a cube
    const float waves = 20.0f * speed * under / side;  // A bobbing body loses energy to surface waves
    force -= drag + waves;
  }
  speed += force / (mass + addedMass) * dt;
  depth += speed * dt;
  if (depth >= floorDepth) {
    depth = floorDepth;
    if (speed > 0) speed = 0;
  }
}

// ---- Magnets: poles as point sources, the field falls with the square of the distance ----
struct PhysicsPole {
  float x, y;
  float strength;  // Positive: north, field points away. Negative: south
};

inline void physicsPoleField(const PhysicsPole* poles, int count, float x, float y, float& bx, float& by) {
  bx = by = 0;
  for (int i = 0; i < count; i++) {
    const float dx = x - poles[i].x, dy = y - poles[i].y;
    const float r2 = dx * dx + dy * dy;
    if (r2 < 1e-3f) continue;
    const float k = poles[i].strength / (r2 * sqrtf(r2));
    bx += k * dx;
    by += k * dy;
  }
}

// How strongly the poles pull at this point, added up without their directions
inline float physicsPolePull(const PhysicsPole* poles, int count, float x, float y) {
  float sum = 0;
  for (int i = 0; i < count; i++) {
    const float dx = x - poles[i].x, dy = y - poles[i].y;
    const float r2 = dx * dx + dy * dy;
    if (r2 >= 1e-3f) sum += fabsf(poles[i].strength) / r2;
  }
  return sum;
}

// Follows the field from a start point in steps, midpoint method. Returns the points written.
// Ends at a south pole, where the field vanishes, outside the bounds or when the buffer is full.
inline int physicsTraceFieldLine(const PhysicsPole* poles, int poleCount, float x, float y, float step, float minX,
                                 float minY, float maxX, float maxY, int16_t* outX, int16_t* outY, int maxPoints) {
  int n = 0;
  while (n < maxPoints) {
    outX[n] = (int16_t)x;
    outY[n] = (int16_t)y;
    n++;
    if (x < minX || y < minY || x > maxX || y > maxY) break;
    bool arrived = false;
    for (int i = 0; i < poleCount; i++) {
      const float dx = x - poles[i].x, dy = y - poles[i].y;
      if (poles[i].strength < 0 && dx * dx + dy * dy < step * step) arrived = true;
    }
    if (arrived) break;
    float bx, by;
    physicsPoleField(poles, poleCount, x, y, bx, by);
    float length = sqrtf(bx * bx + by * by);
    // Poles cancelling out, as between two like poles. Measured against the poles' own pull, as
    // far from a magnet the field is weak but real, and rounding never leaves an exact zero.
    if (length < 1e-4f * physicsPolePull(poles, poleCount, x, y)) break;
    const float mx = x + 0.5f * step * bx / length, my = y + 0.5f * step * by / length;
    physicsPoleField(poles, poleCount, mx, my, bx, by);
    length = sqrtf(bx * bx + by * by);
    if (length <= 0) break;
    x += step * bx / length;
    y += step * by / length;
  }
  return n;
}

// ---- Pendulum: T = 2 pi sqrt(L / g) for small swings; the motion itself is the full equation ----
inline float physicsPendulumPeriod(float length, float g) { return 2.0f * PHYSICS_PI * sqrtf(length / g); }

// One Runge-Kutta step of theta'' = -(g / L) sin(theta)
inline void physicsPendulumStep(float& theta, float& omega, float length, float g, float dt) {
  const float k = g / length;
  const float k1t = omega, k1o = -k * sinf(theta);
  const float k2t = omega + 0.5f * dt * k1o, k2o = -k * sinf(theta + 0.5f * dt * k1t);
  const float k3t = omega + 0.5f * dt * k2o, k3o = -k * sinf(theta + 0.5f * dt * k2t);
  const float k4t = omega + dt * k3o, k4o = -k * sinf(theta + dt * k3t);
  theta += dt / 6.0f * (k1t + 2 * k2t + 2 * k3t + k4t);
  omega += dt / 6.0f * (k1o + 2 * k2o + 2 * k3o + k4o);
}

// ---- Ramp: a ball in a valley y = a x^2 swaps height for speed. mass 1 kg ----
inline float physicsRampHeightEnergy(float x, float a, float g) { return g * a * x * x; }
inline float physicsRampMotionEnergy(float x, float v, float a) {
  const float slope = 2.0f * a * x;
  return 0.5f * v * v * (1.0f + slope * slope);  // v is dx/dt; the ball also moves up and down
}

inline float physicsRampAcceleration(float x, float v, float a, float g, float loss) {
  const float s = 4.0f * a * a;
  return (-2.0f * g * a * x - s * x * v * v) / (1.0f + s * x * x) - loss * v;
}

// Runge-Kutta. loss 0: no friction, energy stays. loss > 0: slows down, the rest becomes heat.
inline void physicsRampStep(float& x, float& v, float a, float g, float loss, float dt) {
  const float k1x = v, k1v = physicsRampAcceleration(x, v, a, g, loss);
  const float k2x = v + 0.5f * dt * k1v, k2v = physicsRampAcceleration(x + 0.5f * dt * k1x, k2x, a, g, loss);
  const float k3x = v + 0.5f * dt * k2v, k3v = physicsRampAcceleration(x + 0.5f * dt * k2x, k3x, a, g, loss);
  const float k4x = v + dt * k3v, k4v = physicsRampAcceleration(x + dt * k3x, k4x, a, g, loss);
  x += dt / 6.0f * (k1x + 2 * k2x + 2 * k3x + k4x);
  v += dt / 6.0f * (k1v + 2 * k2v + 2 * k3v + k4v);
}

// ---- Shadow: light travels straight, so similar triangles give where the shadow ends ----
// A lamp at (lampX, lampY) above the floor, an upright object at objectX of the given height.
// True and the end of the shadow if the lamp is above the object's top; otherwise it never lands.
inline bool physicsShadowEnd(float lampX, float lampY, float objectX, float height, float& endX) {
  if (lampY <= height) return false;
  endX = objectX + (objectX - lampX) * height / (lampY - height);
  return true;
}

// ---- Mirrors: the angle in is the angle out ----
inline void physicsReflect(float dx, float dy, float nx, float ny, float& rx, float& ry) {
  const float d = dx * nx + dy * ny;
  rx = dx - 2.0f * d * nx;
  ry = dy - 2.0f * d * ny;
}

// Where a ray with unit direction hits a circle. second: the far crossing, for a ray inside the
// circle (a hollow mirror). False if it misses.
inline bool physicsRayCircle(float ox, float oy, float dx, float dy, float cx, float cy, float radius, bool second, float& t) {
  const float fx = ox - cx, fy = oy - cy;
  const float b = fx * dx + fy * dy;
  const float c = fx * fx + fy * fy - radius * radius;
  const float disc = b * b - c;
  if (disc < 0) return false;
  const float root = sqrtf(disc);
  t = second ? -b + root : -b - root;
  return t > 0;
}

// ---- Refraction: Snell's law, n1 sin(a1) = n2 sin(a2). Angles in radians from the normal ----
// False: total reflection, no ray gets through.
inline bool physicsSnell(float n1, float n2, float angleIn, float& angleOut) {
  const float s = n1 / n2 * sinf(angleIn);
  if (fabsf(s) > 1.0f) return false;
  angleOut = asinf(s);
  return true;
}

// Beyond this angle everything reflects. Negative when going into a denser material: there is none.
inline float physicsCriticalAngle(float n1, float n2) {
  return n1 > n2 ? asinf(n2 / n1) : -1.0f;
}

// ---- Sound: a pressure wave moving through air ----
static const float PHYSICS_SOUND_SPEED = 343.0f;  // m/s in air at 20 C

inline float physicsWavelength(float speed, float frequency) { return speed / frequency; }
// How far a bit of air is pushed from its place at x, time t
inline float physicsWaveDisplacement(float amplitude, float wavelength, float period, float x, float t) {
  return amplitude * sinf(2.0f * PHYSICS_PI * (x / wavelength - t / period));
}

// Air pressure above normal: p is proportional to -dxi/dx, so it peaks where the air is squeezed,
// a quarter wave from the largest push. Scaled so that its largest value is peak.
inline float physicsWavePressure(float peak, float wavelength, float period, float x, float t) {
  return -peak * cosf(2.0f * PHYSICS_PI * (x / wavelength - t / period));
}

// ---- Circuit: Ohm's law, I = U / R, and bulbs in a row add up their resistance ----
inline float physicsOhmCurrent(float volts, float ohms) { return ohms > 0 ? volts / ohms : 0; }
inline float physicsSeriesResistance(float each, int count) { return each * count; }
inline float physicsPower(float amps, float ohms) { return amps * amps * ohms; }

// ---- States of matter: particles that attract when near and repel when too close ----
// The Lennard-Jones model in its usual units: energy and distance scale 1, mass 1.
#define PHYSICS_MATTER_COUNT 30
static const float PHYSICS_MATTER_DT = 0.005f;
static const float PHYSICS_MATTER_GRAVITY = 0.6f;
static const float PHYSICS_MATTER_COOLING = 0.02f;  // Share of the temperature gap closed per step
static const float PHYSICS_MATTER_SOLID_T = 0.05f;
static const float PHYSICS_MATTER_LIQUID_T = 0.5f;
static const float PHYSICS_MATTER_GAS_T = 3.0f;
static const float PHYSICS_MATTER_WIDTH = 18.0f;
static const float PHYSICS_MATTER_HEIGHT = 7.6f;

struct PhysicsMatter {
  float x[PHYSICS_MATTER_COUNT], y[PHYSICS_MATTER_COUNT];
  float vx[PHYSICS_MATTER_COUNT], vy[PHYSICS_MATTER_COUNT];
  float ax[PHYSICS_MATTER_COUNT], ay[PHYSICS_MATTER_COUNT];
};

inline void physicsMatterForces(PhysicsMatter& m) {
  const float cutoff2 = 2.5f * 2.5f;
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) {
    m.ax[i] = 0;
    m.ay[i] = -PHYSICS_MATTER_GRAVITY;
  }
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) {
    for (int j = i + 1; j < PHYSICS_MATTER_COUNT; j++) {
      const float dx = m.x[i] - m.x[j], dy = m.y[i] - m.y[j];
      float r2 = dx * dx + dy * dy;
      if (r2 > cutoff2) continue;
      if (r2 < 0.5f) r2 = 0.5f;  // Keeps a rare overlap from throwing particles across the box
      const float i2 = 1.0f / r2, i6 = i2 * i2 * i2;
      const float f = 24.0f * i2 * i6 * (2.0f * i6 - 1.0f);
      m.ax[i] += f * dx; m.ay[i] += f * dy;
      m.ax[j] -= f * dx; m.ay[j] -= f * dy;
    }
  }
}

// Starts as a neat crystal resting on the floor, at rest
inline void physicsMatterInit(PhysicsMatter& m) {
  const float spacing = 1.12f;  // Where the attraction and repulsion balance
  const int perRow = 8;
  const float left = (PHYSICS_MATTER_WIDTH - (perRow - 1) * spacing) / 2.0f;
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) {
    const int row = i / perRow, col = i % perRow;
    m.x[i] = left + col * spacing + (row % 2 ? spacing / 2.0f : 0.0f);
    m.y[i] = 0.6f + row * spacing * 0.866f;
    // A tiny, repeatable nudge, so the crystal isn't perfectly still
    m.vx[i] = ((i * 37) % 11 - 5) * 0.02f;
    m.vy[i] = ((i * 53) % 11 - 5) * 0.02f;
  }
  physicsMatterForces(m);
}

inline float physicsMatterTemperature(const PhysicsMatter& m) {
  float sum = 0;
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) sum += m.vx[i] * m.vx[i] + m.vy[i] * m.vy[i];
  return sum / (2.0f * PHYSICS_MATTER_COUNT);  // Two directions: kT = mean of v^2 / 2
}

// One step (velocity Verlet), with hard walls and a heater or cooler pulling to the target temperature
inline void physicsMatterStep(PhysicsMatter& m, float target) {
  const float dt = PHYSICS_MATTER_DT;
  const float low = 0.45f, highX = PHYSICS_MATTER_WIDTH - 0.45f, highY = PHYSICS_MATTER_HEIGHT - 0.45f;
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) {
    m.vx[i] += 0.5f * dt * m.ax[i];
    m.vy[i] += 0.5f * dt * m.ay[i];
    m.x[i] += dt * m.vx[i];
    m.y[i] += dt * m.vy[i];
    if (m.x[i] < low) { m.x[i] = low; if (m.vx[i] < 0) m.vx[i] = -m.vx[i]; }
    if (m.x[i] > highX) { m.x[i] = highX; if (m.vx[i] > 0) m.vx[i] = -m.vx[i]; }
    if (m.y[i] < low) { m.y[i] = low; if (m.vy[i] < 0) m.vy[i] = -m.vy[i]; }
    if (m.y[i] > highY) { m.y[i] = highY; if (m.vy[i] > 0) m.vy[i] = -m.vy[i]; }
  }
  physicsMatterForces(m);
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) {
    m.vx[i] += 0.5f * dt * m.ax[i];
    m.vy[i] += 0.5f * dt * m.ay[i];
  }
  // Standard practice in such simulations: no drift of the whole sample, or a solid would
  // slide along the frictionless floor
  float drift = 0;
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) drift += m.vx[i] / PHYSICS_MATTER_COUNT;
  for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) m.vx[i] -= drift;
  const float now = physicsMatterTemperature(m);
  if (now > 1e-6f) {
    const float scale = sqrtf(1.0f + PHYSICS_MATTER_COOLING * (target / now - 1.0f));
    for (int i = 0; i < PHYSICS_MATTER_COUNT; i++) {
      m.vx[i] *= scale;
      m.vy[i] *= scale;
    }
  }
}

// ---- Date: Julian day from a calendar date or from the clock ----
inline double physicsJulianDay(int year, int month, int day, double hours) {
  if (month <= 2) { year--; month += 12; }
  const int a = year / 100;
  const int b = 2 - a + a / 4;
  return floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + b - 1524.5 + hours / 24.0;
}
inline double physicsJulianDayFromUnix(double seconds) { return 2440587.5 + seconds / 86400.0; }

// ---- Seasons: the Earth's axis stays tilted while it goes around the Sun ----
static const float PHYSICS_EARTH_TILT = 23.44f * PHYSICS_DEG;
static const float PHYSICS_HOME_LATITUDE = 47.0f * PHYSICS_DEG;  // Middle of Switzerland, north

inline float physicsDegrees360(double deg) {
  double r = fmod(deg, 360.0);
  if (r < 0) r += 360.0;
  return (float)r;
}

// Where the Sun appears along the year, in radians: 0 at the March equinox, pi/2 at the June
// solstice. The Astronomical Almanac's low-precision formula.
inline float physicsSunLongitude(double julianDay) {
  const double n = julianDay - 2451545.0;
  const float mean = physicsDegrees360(280.460 + 0.9856474 * n);
  const float g = physicsDegrees360(357.528 + 0.9856003 * n) * PHYSICS_DEG;
  return physicsDegrees360(mean + 1.915 * sin(g) + 0.020 * sin(2 * g)) * PHYSICS_DEG;
}

inline float physicsSunDeclination(float longitude) {
  return asinf(sinf(PHYSICS_EARTH_TILT) * sinf(longitude));
}

// Hours from sunrise to sunset, geometric (Sun's centre on the horizon), from cos H = -tan(lat) tan(decl)
inline float physicsDayHours(float latitude, float declination) {
  const float c = -tanf(latitude) * tanf(declination);
  if (c <= -1.0f) return 24.0f;  // Sun never sets
  if (c >= 1.0f) return 0.0f;    // Sun never rises
  return 24.0f * acosf(c) / PHYSICS_PI;
}

inline float physicsNoonHeight(float latitude, float declination) {
  return PHYSICS_PI / 2.0f - fabsf(latitude - declination);
}

// 0 spring, 1 summer, 2 autumn, 3 winter, in the north
inline int physicsSeasonIndex(float longitude) {
  int season = (int)(longitude / (PHYSICS_PI / 2.0f));
  return season < 0 ? 0 : (season > 3 ? 3 : season);
}

// The Earth's axis as a unit vector, the Sun to the left (-x), looking along the orbit: +y up
inline void physicsEarthAxis(float longitude, float axis[3]) {
  axis[0] = -sinf(PHYSICS_EARTH_TILT) * sinf(longitude);
  axis[1] = cosf(PHYSICS_EARTH_TILT);
  axis[2] = sinf(PHYSICS_EARTH_TILT) * cosf(longitude);
}

// A point on a circle of latitude; t runs around the Earth once from 0 to 2 pi
inline void physicsLatitudePoint(const float axis[3], float latitude, float t, float out[3]) {
  // Two directions at right angles to the axis
  float ux = axis[1], uy = -axis[0], uz = 0;
  const float length = sqrtf(ux * ux + uy * uy);
  ux /= length; uy /= length;
  const float vx = axis[1] * uz - axis[2] * uy, vy = axis[2] * ux - axis[0] * uz, vz = axis[0] * uy - axis[1] * ux;
  const float s = sinf(latitude), c = cosf(latitude), ct = cosf(t), st = sinf(t);
  out[0] = axis[0] * s + c * (ux * ct + vx * st);
  out[1] = axis[1] * s + c * (uy * ct + vy * st);
  out[2] = axis[2] * s + c * (uz * ct + vz * st);
}

// ---- Moon: how far it is from the Sun, seen from Earth. Meeus, Astronomical Algorithms, ch. 48 ----
// 0 new moon, 180 full moon, in degrees. Below 180 it is growing.
inline float physicsMoonElongation(double julianDay) {
  const double t = (julianDay - 2451545.0) / 36525.0;
  const float d = physicsDegrees360(297.8501921 + 445267.1114034 * t) * PHYSICS_DEG;
  const float m = physicsDegrees360(357.5291092 + 35999.0502909 * t) * PHYSICS_DEG;
  const float mp = physicsDegrees360(134.9633964 + 477198.8675055 * t) * PHYSICS_DEG;
  const float correction = 6.289f * sinf(mp) - 2.100f * sinf(m) + 1.274f * sinf(2 * d - mp) + 0.658f * sinf(2 * d) +
                           0.214f * sinf(2 * mp) + 0.110f * sinf(d);
  return physicsDegrees360(d / PHYSICS_DEG + correction);
}

// Lit part of the disc, 0 to 1
inline float physicsMoonIllumination(float elongationDegrees) {
  return (1.0f - cosf(elongationDegrees * PHYSICS_DEG)) / 2.0f;
}

// 0 new, 1 growing crescent, 2 first quarter, 3 growing, 4 full, 5 shrinking, 6 last quarter, 7 shrinking crescent
inline int physicsMoonPhaseIndex(float elongationDegrees) {
  int index = (int)((elongationDegrees + 22.5f) / 45.0f);
  return index % 8;
}

// One line of a disc with a lit part, as seen in the north: a growing moon is lit on the right.
// y from the middle, within radius. Gives the lit stretch, from..to, from the middle line.
inline void physicsLitSpan(float y, float radius, float lit, bool growing, float& from, float& to) {
  const float halfWidth = sqrtf(physicsClamp(radius * radius - y * y, 0.0f, radius * radius));
  const float edge = halfWidth * (1.0f - 2.0f * physicsClamp(lit, 0.0f, 1.0f));
  if (growing) {
    from = edge;
    to = halfWidth;
  } else {
    from = -halfWidth;
    to = -edge;
  }
}

// ---- The scenes: a closed list, in the order a school would teach them ----
enum PhysicsSceneId : uint8_t {
  PHYSICS_FALLING,
  PHYSICS_FRICTION,
  PHYSICS_LEVER,
  PHYSICS_PULLEY,
  PHYSICS_BUOYANCY,
  PHYSICS_PENDULUM,
  PHYSICS_RAMP,
  PHYSICS_MAGNET,
  PHYSICS_SHADOW,
  PHYSICS_MIRROR,
  PHYSICS_REFRACTION,
  PHYSICS_SOUND,
  PHYSICS_CIRCUIT,
  PHYSICS_MATTER,
  PHYSICS_SEASONS,
  PHYSICS_MOON,
  PHYSICS_ATOM
};

struct PhysicsSceneInfo {
  const char* id;
  const char* de;
  const char* en;
  PhysicsSceneId scene;
  uint8_t paramCount;  // Button B steps through these
};

static const PhysicsSceneInfo PHYSICS_SCENES[] = {
    {"falling", "Freier Fall", "Falling", PHYSICS_FALLING, 3},
    {"friction", "Reibung", "Friction", PHYSICS_FRICTION, 3},
    {"lever", "Hebel", "Lever", PHYSICS_LEVER, 5},
    {"pulley", "Flaschenzug", "Pulley", PHYSICS_PULLEY, 3},
    {"buoyancy", "Schwimmen und Sinken", "Float or sink", PHYSICS_BUOYANCY, 5},
    {"pendulum", "Pendel", "Pendulum", PHYSICS_PENDULUM, 3},
    {"ramp", "Energie", "Energy", PHYSICS_RAMP, 2},
    {"magnet", "Magnetfeld", "Magnetic field", PHYSICS_MAGNET, 3},
    {"shadow", "Licht und Schatten", "Light and shadow", PHYSICS_SHADOW, 2},
    {"mirror", "Spiegel", "Mirror", PHYSICS_MIRROR, 3},
    {"refraction", "Lichtbrechung", "Refraction", PHYSICS_REFRACTION, 4},
    {"sound", "Schall", "Sound", PHYSICS_SOUND, 4},
    {"circuit", "Stromkreis", "Circuit", PHYSICS_CIRCUIT, 4},
    {"matter", "Aggregatzust\xC3\xA4nde", "States of matter", PHYSICS_MATTER, 3},
    {"seasons", "Jahreszeiten", "Seasons", PHYSICS_SEASONS, 4},
    {"moon", "Mondphasen", "Moon phases", PHYSICS_MOON, 2},
    {"atom", "Atom", "Atom", PHYSICS_ATOM, PHYSICS_ELEMENT_COUNT},
};
static const int PHYSICS_SCENE_COUNT = sizeof(PHYSICS_SCENES) / sizeof(PHYSICS_SCENES[0]);

// Added to the AI prompt. Every id in it is in PHYSICS_SCENES and the other way round (tested).
static const char PHYSICS_PROMPT_RULE[] =
    "PHYSIK: Setze 'model' auf 'physics:<id>' NUR wenn der Nutzer fragt, wie etwas Physikalisches funktioniert "
    "oder warum es passiert, und eine dieser Szenen genau passt (z.B. 'Wie funktioniert ein Hebel?' ergibt "
    "lever, 'Warum schwimmt Holz?' ergibt buoyancy, 'Warum knickt ein Stab im Wasser?' ergibt refraction, "
    "'Warum gibt es Jahreszeiten?' ergibt seasons, 'Warum sieht der Mond mal rund, mal schmal aus?' ergibt moon). "
    "id genau eine von: falling friction lever pulley buoyancy pendulum ramp magnet shadow mirror refraction "
    "sound circuit matter seasons moon atom-<Symbol>. Symbol ist das Elementsymbol von H bis Kr ('Wie ist ein "
    "Kohlenstoffatom aufgebaut?' ergibt atom-C). Frage nach dem Aufbau eines Atoms ergibt immer physics:atom-<Symbol>, "
    "nie molecule. Allgemeine Fragen ('Was ist ein Magnet?', 'Wer war Newton?') bleiben Fotos mit leerem model. Rechenaufgaben bleiben is_wolfram mit leerem model. "
    "Nenne nur die id, alle Werte berechnet das Geraet.";

// Same letters, whatever the case
inline bool physicsSameLetter(char a, char b) {
  if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
  if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
  return a == b;
}
inline bool physicsSameWordStart(const char* text, const char* word) {
  for (; *word; text++, word++) {
    if (!*text || !physicsSameLetter(*text, *word)) return false;
  }
  return true;
}
inline bool physicsSameWord(const char* text, const char* word) {
  return physicsSameWordStart(text, word) && text[strlen(word)] == 0;
}

// Scene from "<id>", case and spaces ignored. -1 if it isn't on the list.
inline int physicsFindScene(const char* query) {
  if (!query) return -1;
  char clean[32];
  size_t n = 0;
  for (const char* p = query; *p; p++) {
    if (n + 1 >= sizeof(clean)) return -1;
    char c = *p;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c == ' ' || c == '_') c = '-';
    clean[n++] = c;
  }
  clean[n] = 0;
  for (int i = 0; i < PHYSICS_SCENE_COUNT; i++) {
    if (strcmp(PHYSICS_SCENES[i].id, clean) == 0) return i;
  }
  return -1;
}

// ---- Published values the scenes use ----
struct PhysicsWorld { const char* de; const char* en; float g; };
// m/s2: Earth's standard value rounded; Moon and Mars (mean) from NASA's planetary fact sheets
static const PhysicsWorld PHYSICS_WORLDS[3] = {
    {"Erde", "Earth", 9.81f}, {"Mond", "Moon", 1.62f}, {"Mars", "Mars", 3.73f}};

struct PhysicsSurface { const char* de; const char* en; float mu; };
// Typical sliding friction from physics tables; real surfaces vary
static const PhysicsSurface PHYSICS_SURFACES[3] = {
    {"Eis", "Ice", 0.03f}, {"Holz", "Wood", 0.3f}, {"Gummi", "Rubber", 0.8f}};
static const float PHYSICS_PUSH_SPEEDS[3] = {1.0f, 1.5f, 2.0f};  // m/s

struct PhysicsLeverLoad { float mass, marks; };
// Against 2 kg at mark 2 on the left. Some balance, some don't.
static const PhysicsLeverLoad PHYSICS_LEVER_LOADS[5] = {{1, 2}, {1, 4}, {2, 4}, {2, 2}, {4, 1}};
static const float PHYSICS_LEVER_LEFT_MASS = 2, PHYSICS_LEVER_LEFT_MARKS = 2;

static const uint8_t PHYSICS_PULLEY_ROPES[3] = {1, 2, 4};

struct PhysicsMaterial { const char* de; const char* en; float density; };
// g/cm3 from physics tables. Natural materials vary: wood as pine, stone as granite.
static const PhysicsMaterial PHYSICS_MATERIALS[5] = {{"Kork", "Cork", 0.24f},
                                                     {"Holz", "Wood", 0.5f},
                                                     {"Eis", "Ice", 0.92f},
                                                     {"Stein", "Stone", 2.7f},
                                                     {"Eisen", "Iron", 7.87f}};

// Lengths chosen so that one there-and-back takes about 1, 2 and 3 seconds
static const float PHYSICS_PENDULUM_LENGTHS[3] = {0.25f, 1.0f, 2.25f};  // m
static const float PHYSICS_PENDULUM_START = 25.0f * PHYSICS_DEG;

static const float PHYSICS_RAMP_SHAPE = 1.0f;  // y = x^2, metres
static const float PHYSICS_RAMP_START = -0.95f;
static const float PHYSICS_RAMP_LOSS = 0.25f;  // Per second, with friction on

struct PhysicsLightPath {
  const char* upperDe; const char* upperEn; float nUpper;
  const char* lowerDe; const char* lowerEn; float nLower;
  uint8_t tintUpper, tintLower;
};
// Refractive indices from physics tables: yellow light, ordinary glass
static const PhysicsLightPath PHYSICS_LIGHT_PATHS[4] = {
    {"Luft", "Air", 1.0f, "Wasser", "Water", 1.33f, 0, 1},
    {"Luft", "Air", 1.0f, "Glas", "Glass", 1.5f, 0, 2},
    {"Luft", "Air", 1.0f, "Diamant", "Diamond", 2.42f, 0, 3},
    {"Wasser", "Water", 1.33f, "Luft", "Air", 1.0f, 1, 0}};

struct PhysicsTone { const char* de; const char* en; float hertz; float push; };
static const PhysicsTone PHYSICS_TONES[4] = {{"tief, leise", "low, quiet", 220.0f, 0.35f},
                                             {"tief, laut", "low, loud", 220.0f, 0.75f},
                                             {"hoch, leise", "high, quiet", 880.0f, 0.35f},
                                             {"hoch, laut", "high, loud", 880.0f, 0.75f}};
static const float PHYSICS_SOUND_SCREEN_SPEED = 94.0f;  // Pixels per second, all tones alike
static const float PHYSICS_SOUND_LONGEST = 1.0f;        // Seconds, the slowest tone's period on screen

#define PHYSICS_MAGNET_LINES 28
#define PHYSICS_MAGNET_POINTS 100

struct PhysicsCircuit { bool closed; float volts; uint8_t bulbs; };
static const PhysicsCircuit PHYSICS_CIRCUITS[4] = {{false, 4.5f, 1}, {true, 4.5f, 1}, {true, 9.0f, 1}, {true, 4.5f, 2}};
static const float PHYSICS_BULB_OHMS = 15.0f;  // A small torch bulb, taken as constant

// A new moon, Meeus; only used when the device has no clock to say what day it is
static const double PHYSICS_REFERENCE_NEW_MOON = 2451550.26;
// Clock values before this mean the device hasn't got the time yet (2024-01-01)
static const double PHYSICS_CLOCK_VALID_FROM = 1704067200.0;

// Scales an RGB565 colour by 0..1
inline uint16_t physicsShade565(uint16_t color, float brightness) {
  brightness = physicsClamp(brightness, 0.0f, 1.0f);
  const uint16_t r = (uint16_t)(((color >> 11) & 0x1F) * brightness + 0.5f);
  const uint16_t g = (uint16_t)(((color >> 5) & 0x3F) * brightness + 0.5f);
  const uint16_t b = (uint16_t)((color & 0x1F) * brightness + 0.5f);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// The day for the sky scenes, from the clock. False and a fixed known day if it isn't set yet.
inline bool physicsDayFromClock(double unixSeconds, double& julianDay) {
  if (!(unixSeconds >= PHYSICS_CLOCK_VALID_FROM)) {
    julianDay = PHYSICS_REFERENCE_NEW_MOON;
    return false;
  }
  julianDay = physicsJulianDayFromUnix(unixSeconds);
  return true;
}

// Button B in the seasons: on to the start of the next season, just past the line so it counts
inline float physicsNextSeasonLongitude(float longitude) {
  const float quarter = PHYSICS_PI / 2.0f;
  int next = (int)(longitude / quarter) + 1;
  if (next > 3) next = 0;
  return next * quarter + 0.5f * PHYSICS_DEG;
}

// ---- Atoms ----
static const float PHYSICS_ATOM_SPEED = 3.0f;  // Radians per second on the first shell

// "atom-<symbol>", such as atom-C or atom-Fe; English names work too. Element number 1 to
// PHYSICS_ELEMENT_COUNT, or 0 if it isn't one of them.
inline int physicsFindElement(const char* name) {
  if (!name || !name[0]) return 0;
  for (int i = 0; i < PHYSICS_ELEMENT_COUNT; i++) {
    const PhysicsElement& e = PHYSICS_ELEMENTS[i];
    if (physicsSameWord(name, e.symbol) || physicsSameWord(name, e.en)) return i + 1;
  }
  return 0;
}

// Scene and, for atoms, element from the AI's query. False if it isn't on the closed list.
inline bool physicsParseQuery(const char* query, int& sceneIndex, int& element) {
  sceneIndex = -1;
  element = 0;
  if (!query) return false;
  if (physicsSameWordStart(query, "atom-")) {
    element = physicsFindElement(query + 5);
    if (element == 0) return false;
    for (int i = 0; i < PHYSICS_SCENE_COUNT; i++) {
      if (PHYSICS_SCENES[i].scene == PHYSICS_ATOM) sceneIndex = i;
    }
    return sceneIndex >= 0;
  }
  sceneIndex = physicsFindScene(query);
  // The atom scene only comes with an element
  if (sceneIndex >= 0 && PHYSICS_SCENES[sceneIndex].scene == PHYSICS_ATOM) sceneIndex = -1;
  return sceneIndex >= 0;
}
