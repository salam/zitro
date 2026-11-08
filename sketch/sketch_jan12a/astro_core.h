// Sky mechanics for the physics scenes: planets from JPL's Keplerian elements, orbits by Kepler's
// laws, the calendar. Plain C++ with no Arduino includes, tested on the host: tests/physics/run.sh
#pragma once

#include <math.h>
#include <stdint.h>

static const double ASTRO_PI = 3.14159265358979323846;
static const double ASTRO_RAD = ASTRO_PI / 180.0;
static const double ASTRO_J2000 = 2451545.0;  // 2000-01-01 12:00, the elements' epoch
static const double ASTRO_AU_KM = 149597870.7;  // IAU 2012

// 0 to 360
inline double astroWrap360(double deg) {
  double r = fmod(deg, 360.0);
  if (r < 0) r += 360.0;
  return r;
}
// -180 to 180
inline double astroWrap180(double deg) {
  double r = astroWrap360(deg);
  return r > 180.0 ? r - 360.0 : r;
}

// ---- Planets: E.M. Standish, "Keplerian Elements for Approximate Positions of the Major Planets", JPL,
// ssd.jpl.nasa.gov/planets/approx_pos.html, Table 1: mean ecliptic and equinox of J2000, valid 1800 to 2050.
// Each element is its value at J2000 plus its rate per Julian century.
struct AstroPlanet {
  const char* de;
  const char* en;
  double a, e, inclination, meanLongitude, perihelion, node;              // au, -, degrees
  double aRate, eRate, inclinationRate, meanLongitudeRate, perihelionRate, nodeRate;  // per century
  uint16_t color;  // RGB565, for the drawing only
};

enum AstroPlanetId : uint8_t {
  ASTRO_MERCURY,
  ASTRO_VENUS,
  ASTRO_EARTH,  // The Earth-Moon barycentre, as in the table
  ASTRO_MARS,
  ASTRO_JUPITER,
  ASTRO_SATURN,
  ASTRO_URANUS,
  ASTRO_NEPTUNE
};
static const int ASTRO_PLANET_COUNT = 8;

static const AstroPlanet ASTRO_PLANETS[ASTRO_PLANET_COUNT] = {
    {"Merkur", "Mercury", 0.38709927, 0.20563593, 7.00497902, 252.25032350, 77.45779628, 48.33076593,
     0.00000037, 0.00001906, -0.00594749, 149472.67411175, 0.16047689, -0.12534081, 0xAD55},
    {"Venus", "Venus", 0.72333566, 0.00677672, 3.39467605, 181.97909950, 131.60246718, 76.67984255,
     0.00000390, -0.00004107, -0.00078890, 58517.81538729, 0.00268329, -0.27769418, 0xFF13},
    {"Erde", "Earth", 1.00000261, 0.01671123, -0.00001531, 100.46457166, 102.93768193, 0.0,
     0.00000562, -0.00004392, -0.01294668, 35999.37244981, 0.32327364, 0.0, 0x2D7F},
    {"Mars", "Mars", 1.52371034, 0.09339410, 1.84969142, -4.55343205, -23.94362959, 49.55953891,
     0.00001847, 0.00007882, -0.00813131, 19140.30268499, 0.44441088, -0.29257343, 0xF9E7},
    {"Jupiter", "Jupiter", 5.20288700, 0.04838624, 1.30439695, 34.39644051, 14.72847983, 100.47390909,
     -0.00011607, -0.00013253, -0.00183714, 3034.74612775, 0.21252668, 0.20469106, 0xEE0F},
    {"Saturn", "Saturn", 9.53667594, 0.05386179, 2.48599187, 49.95424423, 92.59887831, 113.66242448,
     -0.00125060, -0.00050991, 0.00193609, 1222.49362201, -0.41897216, -0.28867794, 0xF5A0},
    {"Uranus", "Uranus", 19.18916464, 0.04725744, 0.77263783, 313.23810451, 170.95427630, 74.01692503,
     -0.00196176, -0.00004397, -0.00242939, 428.48202785, 0.40805281, 0.04240589, 0x8FFC},
    {"Neptun", "Neptune", 30.06992276, 0.00859048, 1.77004347, -55.12002969, 44.96476227, 131.78422574,
     0.00026291, 0.00005105, 0.00035372, 218.45945325, -0.32241464, -0.00508664, 0x4B7F},
};
// The table's time span, as Julian days: 1800-01-01 and 2050-01-01
static const double ASTRO_ELEMENTS_FROM = 2378496.5;
static const double ASTRO_ELEMENTS_TO = 2469807.5;

// Kepler's equation M = E - e sin E, by Newton's method. Radians.
inline double astroEccentricAnomaly(double meanAnomaly, double e) {
  double E = meanAnomaly + e * sin(meanAnomaly);
  for (int i = 0; i < 30; i++) {
    const double step = (E - e * sin(E) - meanAnomaly) / (1.0 - e * cos(E));
    E -= step;
    if (fabs(step) < 1e-12) break;
  }
  return E;
}

// Where an orbit puts a body: the point in its own plane, x towards the closest point (perihelion)
inline void astroOrbitPlanePoint(double a, double e, double eccentricAnomaly, double& x, double& y) {
  x = a * (cos(eccentricAnomaly) - e);
  y = a * sqrt(1.0 - e * e) * sin(eccentricAnomaly);
}

// A planet's orbit at this day: elements plus their rates, angles in radians, mean anomaly within +-pi
struct AstroOrbitShape { double a, e, inclination, perihelionArgument, node, meanAnomaly; };
inline AstroOrbitShape astroPlanetOrbit(int planet, double julianDay) {
  const AstroPlanet& p = ASTRO_PLANETS[planet];
  const double T = (julianDay - ASTRO_J2000) / 36525.0;
  AstroOrbitShape o;
  o.a = p.a + p.aRate * T;
  o.e = p.e + p.eRate * T;
  o.inclination = (p.inclination + p.inclinationRate * T) * ASTRO_RAD;
  const double meanLongitude = p.meanLongitude + p.meanLongitudeRate * T;
  const double perihelion = p.perihelion + p.perihelionRate * T;
  const double node = p.node + p.nodeRate * T;
  o.perihelionArgument = (perihelion - node) * ASTRO_RAD;
  o.node = node * ASTRO_RAD;
  // Reduced while still in double: L grows by about 150,000 degrees a century
  o.meanAnomaly = astroWrap180(meanLongitude - perihelion) * ASTRO_RAD;
  return o;
}

// The point of that orbit at this eccentric anomaly, in au, J2000 ecliptic: x to the March equinox,
// z to the ecliptic's north
inline void astroOrbitPoint(const AstroOrbitShape& o, double eccentricAnomaly, double out[3]) {
  double x, y;
  astroOrbitPlanePoint(o.a, o.e, eccentricAnomaly, x, y);
  const double cw = cos(o.perihelionArgument), sw = sin(o.perihelionArgument), cn = cos(o.node), sn = sin(o.node);
  const double ci = cos(o.inclination), si = sin(o.inclination);
  out[0] = (cw * cn - sw * sn * ci) * x + (-sw * cn - cw * sn * ci) * y;
  out[1] = (cw * sn + sw * cn * ci) * x + (-sw * sn + cw * cn * ci) * y;
  out[2] = (sw * si) * x + (cw * si) * y;
}

// Heliocentric position in au. julianDay is the TDB day; the clock's UTC is about a minute off, far
// below the table's accuracy.
inline void astroPlanetPosition(int planet, double julianDay, double out[3]) {
  const AstroOrbitShape o = astroPlanetOrbit(planet, julianDay);
  astroOrbitPoint(o, astroEccentricAnomaly(o.meanAnomaly, o.e), out);
}

// As seen from the Earth: ecliptic longitude and latitude in degrees, J2000
inline void astroGeocentric(int planet, double julianDay, double& longitude, double& latitude) {
  double p[3], earth[3];
  astroPlanetPosition(planet, julianDay, p);
  astroPlanetPosition(ASTRO_EARTH, julianDay, earth);
  const double x = p[0] - earth[0], y = p[1] - earth[1], z = p[2] - earth[2];
  longitude = astroWrap360(atan2(y, x) / ASTRO_RAD);
  latitude = atan2(z, sqrt(x * x + y * y)) / ASTRO_RAD;
}

inline double astroHeliocentricLongitude(int planet, double julianDay) {
  double p[3];
  astroPlanetPosition(planet, julianDay, p);
  return astroWrap360(atan2(p[1], p[0]) / ASTRO_RAD);
}

// The next opposition of an outer planet from this day on: Sun, Earth and planet in a line, the Earth
// in the middle, so the planet's heliocentric longitude is the Earth's. Found by stepping and halving.
inline double astroNextOpposition(int planet, double fromDay) {
  const double step = 8.0;  // Days: the gap grows by 0.5 (Mars) to 1 degree (Neptune) a day, far from a jump
  double before = astroWrap180(astroHeliocentricLongitude(ASTRO_EARTH, fromDay) - astroHeliocentricLongitude(planet, fromDay));
  for (double day = fromDay + step; day < fromDay + 900.0; day += step) {
    const double now = astroWrap180(astroHeliocentricLongitude(ASTRO_EARTH, day) - astroHeliocentricLongitude(planet, day));
    // The Earth is faster: the gap grows through zero once a synodic period
    if (before < 0 && now >= 0 && now - before < 90.0) {
      double low = day - step, high = day;
      for (int i = 0; i < 24; i++) {  // Halved down to under a second
        const double middle = (low + high) / 2;
        const double gap = astroWrap180(astroHeliocentricLongitude(ASTRO_EARTH, middle) - astroHeliocentricLongitude(planet, middle));
        if (gap < 0) low = middle;
        else high = middle;
      }
      return (low + high) / 2;
    }
    before = now;
  }
  return fromDay;
}

// ---- Calendar: Julian day to date, Meeus, Astronomical Algorithms, ch. 7 ----
inline void astroCalendarDate(double julianDay, int& year, int& month, int& day) {
  const double shifted = julianDay + 0.5;
  const double z = floor(shifted);
  const double f = shifted - z;
  double a = z;
  if (z >= 2299161) {
    const double alpha = floor((z - 1867216.25) / 36524.25);
    a = z + 1 + alpha - floor(alpha / 4);
  }
  const double b = a + 1524;
  const double c = floor((b - 122.1) / 365.25);
  const double d = floor(365.25 * c);
  const double e = floor((b - d) / 30.6001);
  day = (int)(b - d - floor(30.6001 * e) + f);
  month = (int)(e < 14 ? e - 1 : e - 13);
  year = (int)(month > 2 ? c - 4716 : c - 4715);
}

// ---- Orbits around the Earth: Kepler's third law, T = 2 pi sqrt(a^3 / GM) ----
static const double ASTRO_GM_EARTH = 398600.4418;  // km3/s2, IERS Conventions (2010)
static const double ASTRO_GM_MOON = 4902.800118;   // km3/s2, JPL planetary ephemeris (ssd.jpl.nasa.gov/astro_par.html)
static const double ASTRO_EARTH_RADIUS = 6378.137;  // km at the equator, NASA Earth fact sheet; heights count from here

inline double astroOrbitPeriod(double semiMajorAxisKm, double gm) {
  return 2.0 * ASTRO_PI * sqrt(semiMajorAxisKm * semiMajorAxisKm * semiMajorAxisKm / gm);
}

struct AstroOrbit {
  const char* de;
  const char* en;
  double heightKm;     // Above the equator; for the Moon, its mean distance from the Earth's centre
  bool fromCentre;     // heightKm counts from the Earth's centre
  float inclination;   // Degrees, to the Earth's equator
  float eccentricity;
  bool withMoonMass;   // Two heavy bodies: G (M + m) in Kepler's law
};
// Typical heights and tilts; the Moon's distance and eccentricity from NASA's Moon fact sheet.
static const AstroOrbit ASTRO_ORBITS[4] = {
    {"ISS", "ISS", 420.0, false, 51.6f, 0.0f, false},
    {"GPS", "GPS", 20200.0, false, 55.0f, 0.0f, false},
    {"Meteosat", "Meteosat", 35786.0, false, 0.0f, 0.0f, false},
    {"Mond", "Moon", 384400.0, true, 23.44f, 0.0549f, true},
};

inline double astroOrbitSemiMajorAxis(const AstroOrbit& o) {
  return o.fromCentre ? o.heightKm : ASTRO_EARTH_RADIUS + o.heightKm;
}
inline double astroOrbitPeriodOf(const AstroOrbit& o) {
  return astroOrbitPeriod(astroOrbitSemiMajorAxis(o), ASTRO_GM_EARTH + (o.withMoonMass ? ASTRO_GM_MOON : 0.0));
}

// ---- Sizes: volumetric mean radius in km, NASA planetary fact sheets ----
struct AstroBody {
  const char* de;
  const char* en;
  float radiusKm;
  uint16_t color;
};
enum AstroBodyId : uint8_t { ASTRO_BODY_MERCURY, ASTRO_BODY_VENUS, ASTRO_BODY_EARTH, ASTRO_BODY_MARS, ASTRO_BODY_JUPITER,
                             ASTRO_BODY_SATURN, ASTRO_BODY_URANUS, ASTRO_BODY_NEPTUNE, ASTRO_BODY_MOON, ASTRO_BODY_SUN };
static const AstroBody ASTRO_BODIES[10] = {
    {"Merkur", "Mercury", 2439.7f, 0xAD55}, {"Venus", "Venus", 6051.8f, 0xFF13},   {"Erde", "Earth", 6371.0f, 0x2D7F},
    {"Mars", "Mars", 3389.5f, 0xF9E7},      {"Jupiter", "Jupiter", 69911.0f, 0xEE0F}, {"Saturn", "Saturn", 58232.0f, 0xF5A0},
    {"Uranus", "Uranus", 25362.0f, 0x8FFC}, {"Neptun", "Neptune", 24622.0f, 0x4B7F}, {"Mond", "Moon", 1737.4f, 0xCE79},
    {"Sonne", "Sun", 695700.0f, 0xFE60}};
// Saturn's main rings, from the C ring's inner edge to the A ring's outer edge: NASA's Saturnian rings fact sheet
static const float ASTRO_SATURN_RING_INNER_KM = 74658.0f;
static const float ASTRO_SATURN_RING_OUTER_KM = 136780.0f;
