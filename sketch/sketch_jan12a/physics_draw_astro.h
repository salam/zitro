// Drawing for the planets, retrograde loop, Sun's path, satellite orbits and size comparison.
// Included from physics_draw.h, which holds PhysicsState and the shared helpers.
#pragma once

#include "sky_data.h"

static const char* const PHYSICS_MONTHS_DE[12] = {"Jan.", "Feb.", "M\xC3\xA4rz", "Apr.", "Mai", "Juni",
                                                  "Juli", "Aug.", "Sept.", "Okt.", "Nov.", "Dez."};
static const char* const PHYSICS_MONTHS_EN[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// "März 2027"
inline void physicsMonthText(char* out, size_t size, double julianDay, bool german) {
  int year, month, day;
  astroCalendarDate(julianDay, year, month, day);
  snprintf(out, size, "%s %d", (german ? PHYSICS_MONTHS_DE : PHYSICS_MONTHS_EN)[month - 1], year);
}

// The day the sky scenes start from: today, or with no clock the elements' own epoch, 1 January 2000
inline double physicsSkyBaseDay(const PhysicsState& s) {
  return s.clockKnown ? s.startDay : ASTRO_J2000;
}

// ---- Planets: top view of the solar system ----
inline bool physicsPlanetsAll(const PhysicsState& s) { return s.param == 1 || s.param == 3; }
inline bool physicsPlanetsRunning(const PhysicsState& s) { return s.param >= 2 || !s.clockKnown; }
inline int physicsPlanetsShown(const PhysicsState& s) { return physicsPlanetsAll(s) ? ASTRO_PLANET_COUNT : 4; }

// Inner planets true to scale; all planets with the distance's square root, so Mercury and Neptune both fit
inline void physicsPlanetMap(bool all, const double p[3], int& x, int& y) {
  if (!all) {
    x = 58 + (int)lround(p[0] * 32.0);
    y = 58 - (int)lround(p[1] * 32.0);
    return;
  }
  const double r = sqrt(p[0] * p[0] + p[1] * p[1]);
  const double k = r > 0 ? 48.0 * sqrt(r / 30.4) / r : 0;
  x = 58 + (int)lround(p[0] * k);
  y = 52 - (int)lround(p[1] * k);
}

inline double physicsPlanetsDay(const PhysicsState& s) {
  return physicsSkyBaseDay(s) + s.c;
}

inline void physicsPlanetsBuild(PhysicsState& s) {
  PhysicsOrbitData& d = s.data.orbits;
  const double day = physicsPlanetsDay(s);
  for (int planet = 0; planet < physicsPlanetsShown(s); planet++) {
    const AstroOrbitShape orbit = astroPlanetOrbit(planet, day);
    for (int k = 0; k < PHYSICS_ORBIT_POINTS; k++) {
      double p[3];
      astroOrbitPoint(orbit, 2.0 * ASTRO_PI * k / PHYSICS_ORBIT_POINTS, p);
      int x, y;
      physicsPlanetMap(physicsPlanetsAll(s), p, x, y);
      d.x[planet][k] = (int16_t)x;
      d.y[planet][k] = (int16_t)y;
    }
  }
}

// "228 Mio. km", "4,48 Mrd. km"; English "228M km", "4.48 bn km"
inline void physicsMillionKm(char* out, size_t size, double km, bool german) {
  char number[16];
  if (km >= 1e9) {
    physicsFormatNumber(number, sizeof(number), (float)(km / 1e9), 2, german);
    snprintf(out, size, "%s %s", number, german ? "Mrd. km" : "bn km");
  } else {
    if (german) snprintf(out, size, "%d Mio. km", (int)lround(km / 1e6));
    else snprintf(out, size, "%dM km", (int)lround(km / 1e6));  // "m" would read as metres
  }
}

template <class C>
void physicsDrawPlanets(C& c, PhysicsState& s, float dt) {
  PhysicsOrbitData& d = s.data.orbits;
  const bool all = physicsPlanetsAll(s);
  const int shown = physicsPlanetsShown(s);
  s.t += dt;
  if (s.t > 3600.0f) s.t = 0;
  if (physicsPlanetsRunning(s)) {
    s.c += dt * (all ? PHYSICS_PLANET_DAYS_ALL : PHYSICS_PLANET_DAYS_INNER);
    // JPL's elements hold until 2050: then the run starts over
    if (physicsPlanetsDay(s) > ASTRO_ELEMENTS_TO) s.c = 0;
  }
  const double day = physicsPlanetsDay(s);
  const int sunX = 58, sunY = all ? 52 : 58;
  for (int planet = 0; planet < shown; planet++) {
    for (int k = 0; k < PHYSICS_ORBIT_POINTS; k++) {
      const int j = (k + 1) % PHYSICS_ORBIT_POINTS;
      c.drawLine(d.x[planet][k], d.y[planet][k], d.x[planet][j], d.y[planet][j], PHYSICS_DARK);
    }
  }
  c.fillCircle(sunX, sunY, all ? 2 : 4, PHYSICS_CITRUS);
  // One planet at a time gets its name, round and round
  const int named = (int)(s.t / 2.5f) % shown;
  double namedKm = 0;
  for (int planet = 0; planet < shown; planet++) {
    double p[3];
    astroPlanetPosition(planet, day, p);
    int x, y;
    physicsPlanetMap(all, p, x, y);
    const int radius = all && planet >= ASTRO_JUPITER && planet <= ASTRO_SATURN ? 3 : 2;
    c.fillCircle(x, y, radius, ASTRO_PLANETS[planet].color);
    if (planet == named) {
      c.drawCircle(x, y, radius + 3, PHYSICS_WHITE);
      namedKm = sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]) * ASTRO_AU_KM;
    }
  }

  char text[28];
  const int textX = 122;
  const AstroPlanet& planet = ASTRO_PLANETS[named];
  physicsText(c, s.big, s.german ? planet.de : planet.en, textX, 24, planet.color, textdatum_t::baseline_left);
  if (s.clockKnown) {
    physicsMillionKm(text, sizeof(text), namedKm, s.german);
    physicsText(c, s.normal, text, textX, 46, PHYSICS_WHITE, textdatum_t::baseline_left);
  } else {
    physicsText(c, s.normal, physicsPick(s, "ohne Uhr", "no clock"), textX, 46, PHYSICS_RED, textdatum_t::baseline_left);
  }
  if (physicsPlanetsRunning(s)) {
    physicsMonthText(text, sizeof(text), day, s.german);
    physicsText(c, s.normal, text, textX, 70, PHYSICS_CITRUS, textdatum_t::baseline_left);
    physicsText(c, s.normal, physicsPick(s, "Zeitraffer", "time-lapse"), textX, 94, PHYSICS_GREY, textdatum_t::baseline_left);
  } else {
    physicsText(c, s.normal, physicsPick(s, "heute", "today"), textX, 70, PHYSICS_CITRUS, textdatum_t::baseline_left);
  }
  if (all) {
    c.fillRect(0, 99, PHYSICS_W, PHYSICS_H - 99, PHYSICS_BLACK);
    physicsText(c, s.normal, physicsPick(s, "nicht massstabsgetreu", "not to scale"), 2, 112, PHYSICS_GREY,
                textdatum_t::baseline_left);
  }
}

// ---- Retrograde loop: the planet's way across the stars, seen from the moving Earth ----
static const int PHYSICS_LOOP_SUN_X = 56, PHYSICS_LOOP_SUN_Y = 58;
static const int PHYSICS_LOOP_LEFT = 118, PHYSICS_LOOP_TOP = 28, PHYSICS_LOOP_BOTTOM = 72;

inline int physicsLoopPlanet(const PhysicsState& s) { return s.param == 0 ? ASTRO_MARS : ASTRO_JUPITER; }
inline float physicsLoopScale(const PhysicsState& s) { return s.param == 0 ? 30.0f : 9.2f; }  // Pixels per au

inline void physicsLoopTop(const PhysicsState& s, const double p[3], int& x, int& y) {
  x = PHYSICS_LOOP_SUN_X + (int)lround(p[0] * physicsLoopScale(s));
  y = PHYSICS_LOOP_SUN_Y - (int)lround(p[1] * physicsLoopScale(s));
}

inline void physicsLoopBuild(PhysicsState& s) {
  PhysicsLoopData& d = s.data.loop;
  const int planet = physicsLoopPlanet(s);
  const float half = PHYSICS_LOOP_HALF_SPAN[s.param];
  // The loop around the next opposition; one that is under way now counts too
  double base = physicsSkyBaseDay(s);
  if (base + 900.0 + half > ASTRO_ELEMENTS_TO) base = ASTRO_J2000;
  const double opposition = astroNextOpposition(planet, base - 60.0);
  d.startDay = opposition - half;
  double centre, centreLatitude;
  astroGeocentric(planet, opposition, centre, centreLatitude);
  float low = 0, high = 0, lowB = 90, highB = -90;
  float longitude[PHYSICS_LOOP_POINTS], latitude[PHYSICS_LOOP_POINTS];
  for (int i = 0; i < PHYSICS_LOOP_POINTS; i++) {
    double l, b;
    astroGeocentric(planet, d.startDay + 2.0 * half * i / (PHYSICS_LOOP_POINTS - 1), l, b);
    longitude[i] = (float)astroWrap180(l - centre);
    latitude[i] = (float)b;
    low = fminf(low, longitude[i]);
    high = fmaxf(high, longitude[i]);
    lowB = fminf(lowB, latitude[i]);
    highB = fmaxf(highB, latitude[i]);
  }
  // Same scale both ways, so the loop has its true shape on the sky
  const float width = (float)(PHYSICS_W - 4 - PHYSICS_LOOP_LEFT), height = (float)(PHYSICS_LOOP_BOTTOM - PHYSICS_LOOP_TOP - 8);
  d.perDegree = fminf(width / (high - low + 2.0f), height / (highB - lowB + 1.0f));
  const float midL = (low + high) / 2, midB = (lowB + highB) / 2;
  d.centreLongitude = (float)astroWrap360(centre + midL);
  const float cx = PHYSICS_LOOP_LEFT + width / 2, cy = (PHYSICS_LOOP_TOP + PHYSICS_LOOP_BOTTOM) / 2.0f;
  // East is on the left, as when looking south at the sky
  for (int i = 0; i < PHYSICS_LOOP_POINTS; i++) {
    d.x[i] = (int16_t)lroundf(cx - (longitude[i] - midL) * d.perDegree);
    d.y[i] = (int16_t)lroundf(cy - (latitude[i] - midB) * d.perDegree);
  }
  // The bright stars behind it, from the star map's catalogue, turned from the equator to the ecliptic
  d.starCount = 0;
  const double ce = cos(PHYSICS_OBLIQUITY_J2000), se = sin(PHYSICS_OBLIQUITY_J2000);
  for (int i = 0; i < SKY_STAR_COUNT && d.starCount < PHYSICS_LOOP_STARS; i++) {
    const SkyStar& star = SKY_STARS[i];
    if (star.mag10 > 65) continue;  // Brighter than magnitude 4.5
    const double x = star.x / 32767.0, y = star.y / 32767.0, z = star.z / 32767.0;
    const double ye = y * ce + z * se, ze = -y * se + z * ce;
    const float l = (float)astroWrap180(atan2(ye, x) / ASTRO_RAD - centre) - midL;
    const float b = (float)(asin(fmax(-1.0, fmin(1.0, ze))) / ASTRO_RAD) - midB;
    const int sx = (int)lroundf(cx - l * d.perDegree), sy = (int)lroundf(cy - b * d.perDegree);
    if (sx < PHYSICS_LOOP_LEFT || sx >= PHYSICS_W || sy <= PHYSICS_LOOP_TOP || sy >= PHYSICS_LOOP_BOTTOM) continue;
    d.starX[d.starCount] = (int16_t)sx;
    d.starY[d.starCount] = (int16_t)sy;
    d.starSize[d.starCount] = star.mag10 < 45 ? 1 : 0;  // Brighter than magnitude 2.5: bigger
    d.starCount++;
  }
  // The two orbits, top view
  const int bodies[2] = {ASTRO_EARTH, planet};
  for (int k = 0; k < 2; k++) {
    const AstroOrbitShape orbit = astroPlanetOrbit(bodies[k], opposition);
    for (int i = 0; i < 48; i++) {
      double p[3];
      astroOrbitPoint(orbit, 2.0 * ASTRO_PI * i / 48, p);
      int x, y;
      physicsLoopTop(s, p, x, y);
      d.orbitX[k][i] = (int16_t)x;
      d.orbitY[k][i] = (int16_t)y;
    }
  }
}

template <class C>
void physicsDrawRetrograde(C& c, PhysicsState& s, float dt) {
  const PhysicsLoopData& d = s.data.loop;
  const int planet = physicsLoopPlanet(s);
  const float span = 2.0f * PHYSICS_LOOP_HALF_SPAN[s.param];
  s.c += dt * PHYSICS_LOOP_DAYS_PER_SECOND;
  if (s.c > span + 2.0f * PHYSICS_LOOP_DAYS_PER_SECOND) s.c = 0;  // Two seconds at the end, then again
  const float days = physicsClamp(s.c, 0.0f, span);
  const double day = d.startDay + days;
  const int shown = (int)(days / span * (PHYSICS_LOOP_POINTS - 1));

  // Top view: Sun, the two orbits, Earth and planet, and the line of sight between them
  for (int k = 0; k < 2; k++) {
    for (int i = 0; i < 48; i++) {
      const int j = (i + 1) % 48;
      c.drawLine(d.orbitX[k][i], d.orbitY[k][i], d.orbitX[k][j], d.orbitY[k][j], PHYSICS_DARK);
    }
  }
  c.fillCircle(PHYSICS_LOOP_SUN_X, PHYSICS_LOOP_SUN_Y, s.param == 0 ? 4 : 2, PHYSICS_CITRUS);
  double e[3], p[3];
  astroPlanetPosition(ASTRO_EARTH, day, e);
  astroPlanetPosition(planet, day, p);
  int ex, ey, px, py;
  physicsLoopTop(s, e, ex, ey);
  physicsLoopTop(s, p, px, py);
  const float lx = (float)(px - ex), ly = (float)(py - ey), length = hypotf(lx, ly);
  if (length > 0) {
    const float reach = 120.0f / length;
    c.drawLine(ex, ey, ex + (int)(lx * reach), ey + (int)(ly * reach), PHYSICS_GREY);
  }
  c.fillCircle(ex, ey, 3, ASTRO_PLANETS[ASTRO_EARTH].color);
  c.fillCircle(px, py, 3, ASTRO_PLANETS[planet].color);

  // The sky behind, as seen from the Earth: ecliptic, stars, and the planet's way so far
  c.fillRect(PHYSICS_LOOP_LEFT - 2, PHYSICS_LOOP_TOP, PHYSICS_W - PHYSICS_LOOP_LEFT + 2, PHYSICS_LOOP_BOTTOM - PHYSICS_LOOP_TOP,
             (uint16_t)0x0866);
  for (int i = 0; i < d.starCount; i++) {
    if (d.starSize[i]) c.fillRect(d.starX[i] - 1, d.starY[i] - 1, 2, 2, PHYSICS_WHITE);
    else c.drawPixel(d.starX[i], d.starY[i], PHYSICS_LIGHT);
  }
  for (int i = 1; i <= shown; i++) c.drawLine(d.x[i - 1], d.y[i - 1], d.x[i], d.y[i], ASTRO_PLANETS[planet].color);
  c.fillCircle(d.x[shown], d.y[shown], 2, PHYSICS_WHITE);

  char text[24];
  physicsText(c, s.big, s.german ? ASTRO_PLANETS[planet].de : ASTRO_PLANETS[planet].en, PHYSICS_LOOP_LEFT, 22,
              ASTRO_PLANETS[planet].color, textdatum_t::baseline_left);
  // Backwards: its longitude falls, so it moves to the right on the strip
  const int before = shown > 0 ? shown - 1 : 0, after = shown < PHYSICS_LOOP_POINTS - 1 ? shown + 1 : shown;
  if (d.x[after] > d.x[before]) {
    physicsText(c, s.normal, physicsPick(s, "r\xC3\xBC" "ckl\xC3\xA4ufig", "retrograde"), PHYSICS_LOOP_LEFT, 89, PHYSICS_RED,
                textdatum_t::baseline_left);
  }
  physicsMonthText(text, sizeof(text), day, s.german);
  physicsText(c, s.normal, text, PHYSICS_W - 2, 112, PHYSICS_CITRUS, textdatum_t::baseline_right);
  physicsText(c, s.normal, physicsPick(s, "Zeitraffer", "time-lapse"), 2, 112, PHYSICS_GREY, textdatum_t::baseline_left);
}

// ---- The Sun's path over the home horizon ----
static const float PHYSICS_SUN_PER_DEGREE = 0.85f;
static const int PHYSICS_SUN_HORIZON = 96;

// Facing the equator: south in the north, north in the south. East is left in the north, right in the south.
inline bool physicsSunMap(const PhysicsState& s, float height, float azimuth, float& x, float& y) {
  const float facing = s.homeLatitude >= 0 ? 180.0f : 0.0f;
  const float relative = (float)astroWrap180(azimuth / PHYSICS_DEG - facing);
  x = PHYSICS_W / 2 + relative * PHYSICS_SUN_PER_DEGREE;
  y = PHYSICS_SUN_HORIZON - height / PHYSICS_DEG * PHYSICS_SUN_PER_DEGREE;
  return fabsf(relative) < 150.0f;
}

// A day's path at one declination, above the horizon. thick: the chosen one, with a dot each hour
template <class C>
void physicsDrawSunArc(C& c, const PhysicsState& s, float declination, uint16_t color, bool thick) {
  float lastX = 0, lastY = 0;
  bool lastUp = false;
  for (int step = -120; step <= 120; step++) {
    const float hourAngle = step * 1.5f * PHYSICS_DEG;
    float height, azimuth, x = 0, y = 0;
    physicsSkyPosition(s.homeLatitude, declination, hourAngle, height, azimuth);
    const bool up = height >= 0 && physicsSunMap(s, height, azimuth, x, y);
    if (up && lastUp && fabsf(x - lastX) < 40.0f) {
      if (thick) physicsThickLine(c, (int)lastX, (int)lastY, (int)x, (int)y, color);
      else c.drawLine((int)lastX, (int)lastY, (int)x, (int)y, color);
    }
    if (up && thick && step % 10 == 0) c.fillCircle((int)x, (int)y, 2, PHYSICS_WHITE);  // Every hour of sun time
    lastX = x;
    lastY = y;
    lastUp = up;
  }
}

inline float physicsSunPathDeclination(const PhysicsState& s, int setting) {
  switch (setting) {
    case 1: return PHYSICS_EARTH_TILT;   // June solstice
    case 2: return 0.0f;                 // Equinoxes
    case 3: return -PHYSICS_EARTH_TILT;  // December solstice
    default: return physicsSunDeclination(physicsSunLongitude(s.startDay + s.t / 86400.0));
  }
}

template <class C>
void physicsDrawSunPath(C& c, PhysicsState& s, float dt) {
  s.t += dt;  // Real time: the Sun now moves as slowly as the real one
  c.fillRect(0, PHYSICS_SUN_HORIZON + 1, PHYSICS_W, PHYSICS_H - PHYSICS_SUN_HORIZON - 1, (uint16_t)0x1A02);
  c.drawFastHLine(0, PHYSICS_SUN_HORIZON, PHYSICS_W, PHYSICS_GROUND);
  static const uint16_t colors[4] = {PHYSICS_RED, PHYSICS_ORANGE, PHYSICS_LIGHT, PHYSICS_BLUE};
  for (int k = 1; k <= 3; k++) {
    if (k != s.param) physicsDrawSunArc(c, s, physicsSunPathDeclination(s, k), physicsShade565(colors[k], 0.45f), false);
  }
  const bool today = s.clockKnown;
  if (today && s.param != 0) physicsDrawSunArc(c, s, physicsSunPathDeclination(s, 0), physicsShade565(colors[0], 0.45f), false);
  const bool chosen = s.param != 0 || today;
  const float declination = physicsSunPathDeclination(s, s.param);
  if (chosen) physicsDrawSunArc(c, s, declination, colors[s.param], true);
  // The Sun right now, from the clock; below the horizon it shows dimly in the ground
  if (today) {
    float height, azimuth, x = 0, y = 0;
    physicsSunNow(s.startDay + s.t / 86400.0, s.homeLatitude, s.homeLongitude, height, azimuth);
    if (physicsSunMap(s, height, azimuth, x, y) && y < PHYSICS_H + 4) {
      c.fillCircle((int)x, (int)y, 5, height >= 0 ? PHYSICS_CITRUS : physicsShade565(PHYSICS_CITRUS, 0.4f));
    }
  }

  static const char* const de[4] = {"heute", "Juni", "M\xC3\xA4rz, Sept.", "Dezember"};
  static const char* const en[4] = {"today", "June", "Mar, Sep", "December"};
  char text[24], number[16];
  physicsText(c, s.normal, today || s.param != 0 ? (s.german ? de[s.param] : en[s.param]) : physicsPick(s, "ohne Uhr", "no clock"),
              2, 15, s.param == 0 && !today ? PHYSICS_RED : colors[s.param], textdatum_t::baseline_left);
  physicsLatitudeText(text, sizeof(text), s.homeLatitude, s.german);
  physicsText(c, s.normal, text, PHYSICS_W - 2, 15, PHYSICS_RED, textdatum_t::baseline_right);
  if (chosen) {
    physicsFormatNumber(number, sizeof(number), physicsDayHours(s.homeLatitude, declination), 1, s.german);
    snprintf(text, sizeof(text), "%s %s h", physicsPick(s, "Tag", "Day"), number);
    physicsText(c, s.normal, text, 2, 37, PHYSICS_WHITE, textdatum_t::baseline_left);
    snprintf(text, sizeof(text), "%s %d\xC2\xB0", physicsPick(s, "Mittag", "Noon"),
             (int)lroundf(physicsNoonHeight(s.homeLatitude, declination) / PHYSICS_DEG));
    physicsText(c, s.normal, text, PHYSICS_W - 2, 37, PHYSICS_WHITE, textdatum_t::baseline_right);
  }
  // The directions along the horizon
  const bool north = s.homeLatitude >= 0;
  const int left = (int)(PHYSICS_W / 2 - 90 * PHYSICS_SUN_PER_DEGREE), right = (int)(PHYSICS_W / 2 + 90 * PHYSICS_SUN_PER_DEGREE);
  physicsText(c, s.normal, north ? physicsPick(s, "Osten", "East") : physicsPick(s, "Westen", "West"), left, 112, PHYSICS_LIGHT,
              textdatum_t::baseline_center);
  physicsText(c, s.normal, north ? physicsPick(s, "S\xC3\xBC" "den", "South") : physicsPick(s, "Norden", "North"), PHYSICS_W / 2,
              112, PHYSICS_WHITE, textdatum_t::baseline_center);
  physicsText(c, s.normal, north ? physicsPick(s, "Westen", "West") : physicsPick(s, "Osten", "East"), right, 112, PHYSICS_LIGHT,
              textdatum_t::baseline_center);
}

// ---- Satellite orbits: the Earth and the orbits true to scale ----
static const int PHYSICS_ORBIT_CX = 177, PHYSICS_ORBIT_CY = 58;
static const float PHYSICS_ORBIT_REACH = 58.0f;  // Pixels for the orbit's farthest point
static const float PHYSICS_ORBIT_VIEW = 18.0f * PHYSICS_DEG;   // Seen from a little above the equator
static const float PHYSICS_ORBIT_NODE = 35.0f * PHYSICS_DEG;   // Where the orbit crosses the equator

// A point of an orbit, from its own plane (km) to the screen. depth > 0: in front of the Earth's centre
inline void physicsOrbitProject(float px, float py, float inclination, float scale, float& sx, float& sy, float& depth) {
  // Tilt about the line of nodes, turn about the Earth's axis, then look from a little above
  const float y1 = -py * sinf(inclination), z1 = py * cosf(inclination);
  const float x2 = px * cosf(PHYSICS_ORBIT_NODE) + z1 * sinf(PHYSICS_ORBIT_NODE);
  const float z2 = -px * sinf(PHYSICS_ORBIT_NODE) + z1 * cosf(PHYSICS_ORBIT_NODE);
  const float up = y1 * cosf(PHYSICS_ORBIT_VIEW) - z2 * sinf(PHYSICS_ORBIT_VIEW);
  depth = y1 * sinf(PHYSICS_ORBIT_VIEW) + z2 * cosf(PHYSICS_ORBIT_VIEW);
  sx = PHYSICS_ORBIT_CX + x2 * scale;
  sy = PHYSICS_ORBIT_CY - up * scale;
}

// "92,9 min", "11 h 58 min", "27,3 Tage"
inline void physicsPeriodText(char* out, size_t size, double seconds, bool german) {
  char number[16];
  if (seconds < 7200.0) {
    physicsFormatNumber(number, sizeof(number), (float)(seconds / 60.0), 1, german);
    snprintf(out, size, "%s min", number);
  } else if (seconds < 172800.0) {
    const int minutes = (int)lround(seconds / 60.0);
    snprintf(out, size, "%d h %d min", minutes / 60, minutes % 60);
  } else {
    physicsFormatNumber(number, sizeof(number), (float)(seconds / 86400.0), 1, german);
    snprintf(out, size, "%s %s", number, german ? "Tage" : "days");
  }
}

// "35 786 km"
inline void physicsKmText(char* out, size_t size, double km) {
  const long whole = lround(km);
  if (whole >= 1000) snprintf(out, size, "%ld %03ld km", whole / 1000, whole % 1000);
  else snprintf(out, size, "%ld km", whole);
}

template <class C>
void physicsDrawSatellites(C& c, PhysicsState& s, float dt) {
  const AstroOrbit& orbit = ASTRO_ORBITS[s.param];
  const float a = (float)astroOrbitSemiMajorAxis(orbit), e = orbit.eccentricity;
  const float scale = PHYSICS_ORBIT_REACH / (a * (1.0f + e));  // Pixels per km
  const float earth = (float)ASTRO_EARTH_RADIUS * scale;
  const float inclination = orbit.inclination * PHYSICS_DEG;
  const double period = astroOrbitPeriodOf(orbit);
  const float screenPeriod = PHYSICS_ORBIT_SCREEN_SECONDS[s.param];
  s.t += dt;
  if (s.t >= screenPeriod) s.t -= screenPeriod;

  // The Earth, its equator and a spot on it turning once a sidereal day, at the same pace as the orbit
  const int ex = PHYSICS_ORBIT_CX, ey = PHYSICS_ORBIT_CY;
  const int earthR = earth < 1.0f ? 1 : (int)lroundf(earth);
  c.fillCircle(ex, ey, earthR, PHYSICS_BLUE);
  if (earth >= 6.0f) {
    float lastX = 0, lastY = 0;
    for (int i = 0; i <= 48; i++) {
      const float u = 2.0f * PHYSICS_PI * i / 48;
      float x, y, depth;
      physicsOrbitProject(earth / scale * cosf(u), earth / scale * sinf(u), 0, scale, x, y, depth);
      if (i > 0 && depth > 0) c.drawLine((int)lastX, (int)lastY, (int)x, (int)y, (uint16_t)0x6D7F);
      lastX = x;
      lastY = y;
    }
    const float turn = (float)(2.0 * ASTRO_PI * s.t / screenPeriod * period / PHYSICS_SIDEREAL_DAY);
    float x, y, depth;
    physicsOrbitProject(earth / scale * cosf(turn), earth / scale * sinf(turn), 0, scale, x, y, depth);
    if (depth > 0) c.fillCircle((int)x, (int)y, 2, PHYSICS_RED);
  }
  // The orbit; behind the Earth it is hidden
  float lastX = 0, lastY = 0;
  bool lastSeen = false;
  for (int i = 0; i <= 96; i++) {
    double px, py;
    astroOrbitPlanePoint(a, e, 2.0 * ASTRO_PI * i / 96, px, py);
    float x, y, depth;
    physicsOrbitProject((float)px, (float)py, inclination, scale, x, y, depth);
    const bool seen = depth > 0 || hypotf(x - ex, y - ey) > earth + 1.0f;
    if (i > 0 && seen && lastSeen) c.drawLine((int)lastX, (int)lastY, (int)x, (int)y, PHYSICS_CITRUS);
    lastX = x;
    lastY = y;
    lastSeen = seen;
  }
  // The satellite, by Kepler's second law: faster when closer
  const double meanAnomaly = 2.0 * ASTRO_PI * s.t / screenPeriod;
  double px, py;
  astroOrbitPlanePoint(a, e, astroEccentricAnomaly(meanAnomaly, e), px, py);
  float x, y, depth;
  physicsOrbitProject((float)px, (float)py, inclination, scale, x, y, depth);
  if (depth > 0 || hypotf(x - ex, y - ey) > earth + 1.0f) c.fillCircle((int)x, (int)y, 2, PHYSICS_WHITE);

  char text[28];
  physicsText(c, s.big, s.german ? orbit.de : orbit.en, 2, 24, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsKmText(text, sizeof(text), orbit.heightKm);
  if (!orbit.fromCentre) {
    char line[32];
    snprintf(line, sizeof(line), "%s %s", text, physicsPick(s, "hoch", "up"));
    physicsText(c, s.normal, line, 2, 46, PHYSICS_CITRUS, textdatum_t::baseline_left);
  } else {
    physicsText(c, s.normal, text, 2, 46, PHYSICS_CITRUS, textdatum_t::baseline_left);
  }
  physicsText(c, s.normal, physicsPick(s, "Umlauf", "Orbit"), 2, 70, PHYSICS_LIGHT, textdatum_t::baseline_left);
  physicsPeriodText(text, sizeof(text), period, s.german);
  physicsText(c, s.normal, text, 2, 92, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsText(c, s.normal, physicsPick(s, "Zeitraffer", "time-lapse"), 2, 112, PHYSICS_GREY, textdatum_t::baseline_left);
}

// ---- Sizes: bodies side by side, true to scale with each other ----
struct PhysicsSizePair { uint8_t big, small; };
static const PhysicsSizePair PHYSICS_SIZE_PAIRS[4] = {{ASTRO_BODY_EARTH, ASTRO_BODY_MOON},
                                                      {ASTRO_BODY_JUPITER, ASTRO_BODY_EARTH},
                                                      {ASTRO_BODY_SUN, ASTRO_BODY_JUPITER},
                                                      {ASTRO_BODY_SUN, ASTRO_BODY_EARTH}};

template <class C>
void physicsDrawSaturnRings(C& c, float x, float y, float perKm, uint16_t color) {
  for (float km : {ASTRO_SATURN_RING_INNER_KM, ASTRO_SATURN_RING_OUTER_KM}) {
    const float rx = km * perKm, ry = rx * 0.3f;
    int lastX = 0, lastY = 0;
    for (int i = 0; i <= 32; i++) {
      const float a = 2.0f * PHYSICS_PI * i / 32;
      const int px = (int)lroundf(x + rx * cosf(a)), py = (int)lroundf(y + ry * sinf(a));
      if (i > 0) c.drawLine(lastX, lastY, px, py, color);
      lastX = px;
      lastY = py;
    }
  }
}

template <class C>
void physicsDrawSizes(C& c, PhysicsState& s, float dt) {
  s.t += dt;
  // They grow in together for a moment after each press, keeping their sizes to each other
  const float grow = physicsClamp(s.t / 0.8f, 0.05f, 1.0f);
  const int middle = 52;
  char text[24], number[16];
  if (s.param < 4) {
    const AstroBody& big = ASTRO_BODIES[PHYSICS_SIZE_PAIRS[s.param].big];
    const AstroBody& small = ASTRO_BODIES[PHYSICS_SIZE_PAIRS[s.param].small];
    const float ratio = big.radiusKm / small.radiusKm;
    // The Sun beside the Earth doesn't fit: only its edge shows, the Earth two pixels across
    const float bigR = (s.param == 3 ? 2.0f * ratio : 46.0f) * grow;
    const float smallR = bigR / ratio;
    const float bigX = s.param == 3 ? 70.0f - bigR : 56.0f, smallX = s.param == 3 ? 150.0f : 140.0f;
    c.fillCircle((int)bigX, middle, (int)lroundf(bigR), big.color);
    c.fillCircle((int)smallX, middle, smallR < 1.0f ? 1 : (int)lroundf(smallR), small.color);
    physicsFormatNumber(number, sizeof(number), ratio, ratio < 20.0f ? 2 : 1, s.german);
    snprintf(text, sizeof(text), "%s \xC3\x97", number);
    physicsText(c, s.big, text, PHYSICS_W - 2, 26, PHYSICS_WHITE, textdatum_t::baseline_right);
    // The Sun's name sits on the Sun itself when only its edge shows
    physicsText(c, s.normal, s.german ? big.de : big.en, s.param == 3 ? 4 : (int)bigX, s.param == 3 ? 58 : 112,
                s.param == 3 ? PHYSICS_BLACK : big.color, s.param == 3 ? textdatum_t::baseline_left : textdatum_t::baseline_center);
    physicsText(c, s.normal, s.german ? small.de : small.en, (int)smallX, 112, small.color, textdatum_t::baseline_center);
    return;
  }
  // All eight planets in a row, Jupiter 42 pixels across
  const float perKm = 21.0f / ASTRO_BODIES[ASTRO_BODY_JUPITER].radiusKm * grow;
  float x = 6.0f;
  for (int i = ASTRO_BODY_MERCURY; i <= ASTRO_BODY_NEPTUNE; i++) {
    const float r = ASTRO_BODIES[i].radiusKm * perKm;
    x += r;
    c.fillCircle((int)lroundf(x), middle, r < 1.0f ? 1 : (int)lroundf(r), ASTRO_BODIES[i].color);
    if (i == ASTRO_BODY_SATURN) physicsDrawSaturnRings(c, x, (float)middle, perKm, PHYSICS_LIGHT);
    // Saturn's rings reach well beyond it
    x += r + (i == ASTRO_BODY_JUPITER || i == ASTRO_BODY_SATURN ? 26.0f : 7.0f) * grow;
  }
  physicsText(c, s.normal, physicsPick(s, "Merkur bis Neptun", "Mercury to Neptune"), PHYSICS_W / 2, 112, PHYSICS_WHITE,
              textdatum_t::baseline_center);
}
