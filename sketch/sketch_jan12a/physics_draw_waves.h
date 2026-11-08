// Drawing for the double pendulum, Doppler, double slit, prism and half-life scenes.
// Included from physics_draw.h, which holds PhysicsState and the shared helpers.
#pragma once

// A circle as a polygon: cheap at any radius, and clipped by the canvas
template <class C>
void physicsRing(C& c, float cx, float cy, float radius, uint16_t color) {
  int segments = (int)(radius / 3.0f);
  segments = segments < 16 ? 16 : (segments > 96 ? 96 : segments);
  int lastX = (int)(cx + radius), lastY = (int)cy;
  for (int i = 1; i <= segments; i++) {
    const float a = 2.0f * PHYSICS_PI * i / segments;
    const int x = (int)(cx + radius * cosf(a)), y = (int)(cy + radius * sinf(a));
    c.drawLine(lastX, lastY, x, y, color);
    lastX = x;
    lastY = y;
  }
}

// ---- Double pendulum ----
inline void physicsChaosStart(PhysicsState& s) {
  PhysicsChaosData& d = s.data.chaos;
  const float start = PHYSICS_CHAOS_STARTS[s.param] * PHYSICS_DEG;
  d.pendulum[0] = {start, 0, start, 0};
  d.pendulum[1] = {start + PHYSICS_CHAOS_NUDGE, 0, start, 0};
  d.trailAt = d.trailCount = 0;
}

template <class C>
void physicsDrawChaos(C& c, PhysicsState& s, float dt) {
  static const uint16_t colors[2] = {PHYSICS_CITRUS, PHYSICS_BLUE};
  static const uint16_t trails[2] = {0x6A80, 0x1149};
  PhysicsChaosData& d = s.data.chaos;
  const float l = PHYSICS_CHAOS_LENGTH, g = PHYSICS_G_EARTH, step = 0.002f;
  const int pivotX = 62, pivotY = 58;
  const float perMetre = 108.0f;  // The rods are 27 pixels
  // Real time. s.c: seconds since the start; after a minute it starts over
  for (s.t += dt; s.t >= step; s.t -= step) {
    for (int k = 0; k < 2; k++) physicsDoublePendulumStep(d.pendulum[k], 1.0f, 1.0f, l, l, g, step);
    s.c += step;
  }
  if (s.c > 60.0f) {
    physicsChaosStart(s);
    s.c = 0;
  }
  float tipX[2], tipY[2];
  int elbowX[2], elbowY[2];
  for (int k = 0; k < 2; k++) {
    const PhysicsDoublePendulum& p = d.pendulum[k];
    const float x1 = l * sinf(p.angle1), y1 = l * cosf(p.angle1);
    tipX[k] = x1 + l * sinf(p.angle2);
    tipY[k] = y1 + l * cosf(p.angle2);
    elbowX[k] = pivotX + (int)(x1 * perMetre);
    elbowY[k] = pivotY + (int)(y1 * perMetre);
    d.trailX[k][d.trailAt] = (int16_t)(pivotX + tipX[k] * perMetre);
    d.trailY[k][d.trailAt] = (int16_t)(pivotY + tipY[k] * perMetre);
  }
  d.trailAt = (uint8_t)((d.trailAt + 1) % PHYSICS_CHAOS_TRAIL);
  if (d.trailCount < PHYSICS_CHAOS_TRAIL) d.trailCount++;

  // Where the lower weights have just been
  for (int k = 1; k >= 0; k--) {
    for (int i = 1; i < d.trailCount; i++) {
      const int a = (d.trailAt - i + PHYSICS_CHAOS_TRAIL) % PHYSICS_CHAOS_TRAIL;
      const int b = (d.trailAt - i - 1 + PHYSICS_CHAOS_TRAIL) % PHYSICS_CHAOS_TRAIL;
      c.drawLine(d.trailX[k][a], d.trailY[k][a], d.trailX[k][b], d.trailY[k][b], trails[k]);
    }
  }
  c.fillRect(pivotX - 12, pivotY - 2, 24, 3, PHYSICS_GREY);
  for (int k = 1; k >= 0; k--) {
    const int tx = pivotX + (int)(tipX[k] * perMetre), ty = pivotY + (int)(tipY[k] * perMetre);
    physicsThickLine(c, pivotX, pivotY, elbowX[k], elbowY[k], colors[k]);
    physicsThickLine(c, elbowX[k], elbowY[k], tx, ty, colors[k]);
    c.fillCircle(elbowX[k], elbowY[k], 4, colors[k]);
    c.fillCircle(tx, ty, 4, colors[k]);
  }
  c.fillCircle(pivotX, pivotY, 2, PHYSICS_WHITE);

  char text[24], number[16];
  const int textX = 132;
  snprintf(text, sizeof(text), "%d\xC2\xB0", (int)PHYSICS_CHAOS_STARTS[s.param]);
  physicsText(c, s.big, text, textX, 24, PHYSICS_CITRUS, textdatum_t::baseline_left);
  physicsFormatNumber(number, sizeof(number), PHYSICS_CHAOS_NUDGE / PHYSICS_DEG, 3, s.german);
  snprintf(text, sizeof(text), "+%s\xC2\xB0", number);
  physicsText(c, s.normal, text, textX, 46, PHYSICS_BLUE, textdatum_t::baseline_left);
  physicsText(c, s.normal, physicsPick(s, "Abstand", "Apart"), textX, 70, PHYSICS_LIGHT, textdatum_t::baseline_left);
  const float gap = hypotf(tipX[0] - tipX[1], tipY[0] - tipY[1]) * 100.0f;  // cm
  physicsValue(text, sizeof(text), gap, gap < 9.95f ? 1 : 0, "cm", s.german);
  physicsText(c, s.big, text, textX, 96, PHYSICS_WHITE, textdatum_t::baseline_left);
}

// ---- Doppler effect ----
static const float PHYSICS_DOPPLER_TRACK_Y = 40.0f;
static const float PHYSICS_DOPPLER_EAR_X = 120.0f, PHYSICS_DOPPLER_EAR_Y = 84.0f;
static const float PHYSICS_DOPPLER_START_X = -30.0f;

inline float physicsDopplerScreenSpeed(const PhysicsState& s) {
  return PHYSICS_DOPPLER_KMH[s.param] / 3.6f / PHYSICS_SOUND_SPEED * PHYSICS_DOPPLER_SCREEN_C;
}
inline float physicsDopplerStartX(const PhysicsState& s) {
  return PHYSICS_DOPPLER_KMH[s.param] > 0 ? PHYSICS_DOPPLER_START_X : PHYSICS_DOPPLER_EAR_X;
}

template <class C>
void physicsDrawDoppler(C& c, PhysicsState& s, float dt) {
  PhysicsDopplerData& d = s.data.doppler;
  const float v = physicsDopplerScreenSpeed(s), cScreen = PHYSICS_DOPPLER_SCREEN_C;
  const float vReal = PHYSICS_DOPPLER_KMH[s.param] / 3.6f;
  const float startX = physicsDopplerStartX(s);
  const float ly = PHYSICS_DOPPLER_EAR_Y - PHYSICS_DOPPLER_TRACK_Y;
  s.t += dt;
  // A moving siren passes and comes again; a standing one sends forever, so the clock restarts now and then
  const float run = v > 0 ? (300.0f - startX) / v : 60.0f;
  if (s.t > run) {
    s.t = 0;
    d.nextCrest = 0;
  }
  // Crests that reach the listener now: each one is heard at its own pitch
  bool arrived = false;
  while ((float)d.nextCrest * PHYSICS_DOPPLER_CREST <= s.t) {
    float cosAngle;
    const float sent = d.nextCrest * PHYSICS_DOPPLER_CREST;
    const float arrival = physicsDopplerArrival(sent, startX, v, cScreen, PHYSICS_DOPPLER_EAR_X, ly, cosAngle);
    if (arrival > s.t) break;
    d.heardHz = physicsDopplerHeard(PHYSICS_SIREN_HZ, PHYSICS_SOUND_SPEED, vReal, cosAngle);
    d.nextCrest++;
    arrived = true;
  }
  if (arrived) {
    s.toneHz = (uint16_t)(d.heardHz + 0.5f);
    s.toneMs = 60;
  }

  c.drawFastHLine(0, (int)PHYSICS_DOPPLER_TRACK_Y + 7, PHYSICS_W, PHYSICS_DARK);
  // Crests sent so far, still on screen
  const float far = 300.0f;
  int first = (int)((s.t - far / cScreen) / PHYSICS_DOPPLER_CREST);
  if (first < 0) first = 0;
  for (int i = first; i * PHYSICS_DOPPLER_CREST <= s.t; i++) {
    const float sent = i * PHYSICS_DOPPLER_CREST;
    const float radius = cScreen * (s.t - sent);
    if (radius < 1.0f) continue;
    physicsRing(c, startX + v * sent, PHYSICS_DOPPLER_TRACK_Y, radius, PHYSICS_GREY);
  }
  // The siren, blue light blinking
  const int sx = (int)(startX + v * s.t), sy = (int)PHYSICS_DOPPLER_TRACK_Y;
  c.fillRect(sx - 9, sy - 4, 18, 9, PHYSICS_WHITE);
  c.fillRect(sx - 9, sy, 18, 2, PHYSICS_RED);
  c.fillCircle(sx, sy - 6, 2, ((int)(s.t * 4) % 2) ? PHYSICS_BLUE : PHYSICS_DARK);
  // The listener
  const int ex = (int)PHYSICS_DOPPLER_EAR_X, ey = (int)PHYSICS_DOPPLER_EAR_Y;
  c.fillCircle(ex, ey, 5, PHYSICS_ORANGE);
  c.fillRect(ex - 4, ey + 6, 9, 10, PHYSICS_ORANGE);
  if (arrived) physicsRing(c, (float)ex, (float)ey, 9.0f, PHYSICS_CITRUS);

  char text[24];
  physicsText(c, s.normal, physicsPick(s, "Zeitlupe", "slowed"), 2, 15, PHYSICS_GREY, textdatum_t::baseline_left);
  physicsValue(text, sizeof(text), PHYSICS_SIREN_HZ, 0, "Hz", s.german);
  physicsText(c, s.normal, text, PHYSICS_W - 2, 15, PHYSICS_WHITE, textdatum_t::baseline_right);
  if (PHYSICS_DOPPLER_KMH[s.param] > 0) {
    physicsValue(text, sizeof(text), PHYSICS_DOPPLER_KMH[s.param], 0, "km/h", s.german);
  } else {
    snprintf(text, sizeof(text), "%s", physicsPick(s, "steht", "standing"));
  }
  physicsText(c, s.normal, text, 2, 112, PHYSICS_WHITE, textdatum_t::baseline_left);
  if (d.heardHz > 0) {
    const uint16_t color = d.heardHz > PHYSICS_SIREN_HZ + 0.5f ? PHYSICS_CITRUS
                           : (d.heardHz < PHYSICS_SIREN_HZ - 0.5f ? PHYSICS_BLUE : PHYSICS_WHITE);
    physicsValue(text, sizeof(text), d.heardHz, 0, "Hz", s.german);
    physicsText(c, s.big, text, PHYSICS_W - 2, 112, color, textdatum_t::baseline_right);
  }
}

// ---- Double slit ----
static const int PHYSICS_SLIT_LEFT = 20;  // The wall with the two slits
static const int PHYSICS_SLIT_SCREEN_X = PHYSICS_SLIT_LEFT + 2 * PHYSICS_SLIT_COLUMNS + 2;
static const float PHYSICS_SLIT_GAP = 40.0f;  // Pixels between the slits

inline float physicsSlitWavelength(const PhysicsState& s) {
  return PHYSICS_LASERS[s.param].nanometres / PHYSICS_SLIT_NM_PER_PIXEL;
}

// The two waves at each block of the field, once per setting: only their phase moves with time
inline void physicsSlitBuild(PhysicsState& s) {
  PhysicsSlitData& d = s.data.slit;
  const float lambda = physicsSlitWavelength(s);
  const float middle = PHYSICS_SLIT_ROWS;  // Pixels: the field is twice its rows high
  const float y1 = middle - PHYSICS_SLIT_GAP / 2, y2 = middle + PHYSICS_SLIT_GAP / 2;
  for (int row = 0; row < PHYSICS_SLIT_ROWS; row++) {
    for (int col = 0; col < PHYSICS_SLIT_COLUMNS; col++) {
      const float x = 2.0f * col + 1.0f, y = 2.0f * row + 1.0f;
      const float r1 = hypotf(x, y - y1), r2 = hypotf(x, y - y2);
      // cos(k r1 - w t) + cos(k r2 - w t) = 2 cos(k (r1 - r2) / 2) cos(k (r1 + r2) / 2 - w t)
      d.amplitude[row][col] = (int8_t)(64.0f * cosf(PHYSICS_PI * (r1 - r2) / lambda));
      const float turns = (r1 + r2) / (2.0f * lambda);
      d.phase[row][col] = (uint8_t)((int)((turns - floorf(turns)) * 256.0f) & 255);
    }
  }
  for (int i = 0; i < 256; i++) d.cosine[i] = (int8_t)(64.0f * cosf(2.0f * PHYSICS_PI * i / 256.0f));
  const float screen = (float)(PHYSICS_SLIT_SCREEN_X - PHYSICS_SLIT_LEFT);
  for (int y = 0; y < PHYSICS_H; y++) {
    const float r1 = hypotf(screen, y + 0.5f - y1), r2 = hypotf(screen, y + 0.5f - y2);
    d.band[y] = (uint8_t)(physicsTwoWaveBrightness(r1, r2, lambda) / 4.0f * 255.0f);
  }
}

template <class C>
void physicsDrawDoubleSlit(C& c, PhysicsState& s, float dt) {
  const PhysicsSlitData& d = s.data.slit;
  const PhysicsLight& light = PHYSICS_LASERS[s.param];
  const float lambda = physicsSlitWavelength(s);
  s.t += dt;
  if (s.t >= 1.0f) s.t -= 1.0f;  // One wave a second on screen
  const int now = (int)(s.t * 256.0f);
  uint16_t palette[17];
  for (int i = 0; i <= 16; i++) palette[i] = physicsShade565(light.color, i / 16.0f);
  const int height = 2 * PHYSICS_SLIT_ROWS;
  // The light coming in, a plain wave
  for (int x = 0; x < PHYSICS_SLIT_LEFT - 2; x += 2) {
    const float wave = cosf(2.0f * PHYSICS_PI * (x / lambda - s.t));
    c.fillRect(x, 0, 2, height, palette[(int)(8.0f + 8.0f * wave)]);
  }
  // Behind the slits, the two waves add up
  for (int row = 0; row < PHYSICS_SLIT_ROWS; row++) {
    for (int col = 0; col < PHYSICS_SLIT_COLUMNS; col++) {
      const int wave = d.amplitude[row][col] * d.cosine[(d.phase[row][col] - now) & 255];  // -4096..4096
      c.fillRect(PHYSICS_SLIT_LEFT + 2 * col, 2 * row, 2, 2, palette[8 + wave / 512]);
    }
  }
  // The wall with its two slits
  const int middle = height / 2, gap = (int)PHYSICS_SLIT_GAP;
  c.fillRect(PHYSICS_SLIT_LEFT - 2, 0, 3, height, PHYSICS_LIGHT);
  c.fillRect(PHYSICS_SLIT_LEFT - 2, middle - gap / 2 - 1, 3, 3, PHYSICS_BLACK);
  c.fillRect(PHYSICS_SLIT_LEFT - 2, middle + gap / 2 - 1, 3, 3, PHYSICS_BLACK);
  // The screen: bright and dark bands, and how bright along it
  for (int y = 0; y < height; y++) {
    const float bright = d.band[y] / 255.0f;
    c.drawFastHLine(PHYSICS_SLIT_SCREEN_X, y, 6, physicsShade565(light.color, bright));
    c.drawFastHLine(PHYSICS_SLIT_SCREEN_X + 8, y, 1 + (int)(bright * (PHYSICS_W - PHYSICS_SLIT_SCREEN_X - 10)), PHYSICS_DARK);
  }

  char text[24];
  snprintf(text, sizeof(text), "%s %d nm", s.german ? light.de : light.en, (int)light.nanometres);
  physicsText(c, s.normal, text, 2, 92, light.color, textdatum_t::baseline_left);
  physicsText(c, s.normal, physicsPick(s, "Zeitlupe", "slowed"), PHYSICS_W - 2, 92, PHYSICS_GREY, textdatum_t::baseline_right);
  physicsText(c, s.normal, physicsPick(s, "nicht massstabsgetreu", "not to scale"), 2, 112, PHYSICS_GREY,
              textdatum_t::baseline_left);
}

// ---- Prism ----
static const float PHYSICS_PRISM_X[3] = {110.0f, 76.0f, 144.0f};
static const float PHYSICS_PRISM_Y[3] = {20.0f, 78.89f, 78.89f};  // 68 pixels a side, apex up

template <class C>
void physicsDrawPrism(C& c, PhysicsState& s, float dt) {
  static const uint16_t fills[3] = {0x0B38, 0x2967, 0x31A9};
  const PhysicsGlass& glass = PHYSICS_GLASSES[s.param];
  s.t += dt;
  if (s.t > 1000.0f) s.t = 0;
  c.fillTriangle((int)PHYSICS_PRISM_X[0], (int)PHYSICS_PRISM_Y[0], (int)PHYSICS_PRISM_X[1], (int)PHYSICS_PRISM_Y[1],
                 (int)PHYSICS_PRISM_X[2], (int)PHYSICS_PRISM_Y[2], fills[s.param]);
  for (int i = 0; i < 3; i++) {
    const int j = (i + 1) % 3;
    c.drawLine((int)PHYSICS_PRISM_X[i], (int)PHYSICS_PRISM_Y[i], (int)PHYSICS_PRISM_X[j], (int)PHYSICS_PRISM_Y[j], PHYSICS_LIGHT);
  }
  // White light, rising 30 degrees, meets the left face's middle at 60 degrees from its normal
  const float dx = cosf(30.0f * PHYSICS_DEG), dy = -sinf(30.0f * PHYSICS_DEG);
  const float mx = (PHYSICS_PRISM_X[0] + PHYSICS_PRISM_X[1]) / 2, my = (PHYSICS_PRISM_Y[0] + PHYSICS_PRISM_Y[1]) / 2;
  const float back = mx / dx;
  physicsDrawRay(c, mx - back * dx, my - back * dy, mx, my, s.t * 0.6f, PHYSICS_WHITE);
  float bent[7];
  int through = 0;
  for (int i = 0; i < 7; i++) {
    const float n = physicsSellmeier(glass, PHYSICS_PRISM_NM[i] / 1000.0f);
    float inX, inY, outX, outY, outDx, outDy;
    bent[i] = -1;
    if (!physicsPrismRay(PHYSICS_PRISM_X, PHYSICS_PRISM_Y, n, mx - back * dx, my - back * dy, dx, dy, inX, inY, outX, outY,
                         outDx, outDy)) {
      continue;
    }
    through++;
    bent[i] = atan2f(-dy, dx) - atan2f(-outDy, outDx);
    c.drawLine((int)inX, (int)inY, (int)outX, (int)outY, PHYSICS_PRISM_COLORS[i]);
    const float reach = 260.0f;
    c.drawLine((int)outX, (int)outY, (int)(outX + reach * outDx), (int)(outY + reach * outDy), PHYSICS_PRISM_COLORS[i]);
    c.drawLine((int)outX, (int)outY + 1, (int)(outX + reach * outDx), (int)(outY + reach * outDy) + 1, PHYSICS_PRISM_COLORS[i]);
    // A pulse of light runs along each colour
    const float at = s.t * 0.6f - floorf(s.t * 0.6f);
    c.fillCircle((int)(outX + at * 120.0f * outDx), (int)(outY + at * 120.0f * outDy), 1, PHYSICS_WHITE);
  }

  char text[28], number[16];
  physicsText(c, s.normal, s.german ? glass.de : glass.en, 2, 15, PHYSICS_WHITE, textdatum_t::baseline_left);
  if (glass.code) physicsText(c, s.normal, glass.code, 2, 37, PHYSICS_GREY, textdatum_t::baseline_left);
  // How far violet and red part: the fan of colours, in degrees
  if (through == 7) {
    physicsFormatNumber(number, sizeof(number), (bent[6] - bent[0]) / PHYSICS_DEG, 1, s.german);
    snprintf(text, sizeof(text), "%s %s\xC2\xB0", physicsPick(s, "F\xC3\xA4" "cher", "Spread"), number);
    physicsText(c, s.normal, text, PHYSICS_W - 2, 15, PHYSICS_CITRUS, textdatum_t::baseline_right);
  }
}

// ---- Half-life ----
inline void physicsDecayStart(PhysicsState& s) {
  PhysicsDecayData& d = s.data.decay;
  d.count = PHYSICS_DECAY_ROWS * PHYSICS_DECAY_COLUMNS;
  for (int i = 0; i < d.count; i++) d.alive[i] = 1;
  for (int i = 0; i < PHYSICS_DECAY_CURVE; i++) d.curve[i] = 0;
  d.curve[0] = (uint16_t)d.count;
  s.c = 0;
}

template <class C>
void physicsDrawHalfLife(C& c, PhysicsState& s, float dt) {
  PhysicsDecayData& d = s.data.decay;
  const PhysicsIsotope& isotope = PHYSICS_ISOTOPES[s.param];
  const float halfLife = PHYSICS_DECAY_SCREEN_HALF_LIFE, span = 5.0f * halfLife;
  const int total = PHYSICS_DECAY_ROWS * PHYSICS_DECAY_COLUMNS;
  s.c += dt;
  if (s.c > span + 2.0f) physicsDecayStart(s);
  const bool running = s.c <= span;
  // Each atom on its own, by chance: the same chance in every moment
  const float chance = running ? physicsDecayChance(dt, halfLife) : 0.0f;
  int decayed = 0;
  for (int i = 0; i < total; i++) {
    if (d.alive[i] == 1 && physicsRandom(s.random) < chance) {
      d.alive[i] = 2;  // Just now: drawn bright for this frame
      d.count--;
      decayed++;
    }
  }
  // A Geiger counter clicks once a frame at most
  if (decayed > 0) {
    s.toneHz = 3000;
    s.toneMs = 3;
  }
  const int sample = (int)(physicsClamp(s.c / span, 0.0f, 1.0f) * (PHYSICS_DECAY_CURVE - 1));
  if (running) d.curve[sample] = (uint16_t)d.count;

  for (int i = 0; i < total; i++) {
    const int x = 2 + (i % PHYSICS_DECAY_COLUMNS) * 6, y = 2 + (i / PHYSICS_DECAY_COLUMNS) * 6;
    const uint16_t color = d.alive[i] == 1 ? PHYSICS_GREEN : (d.alive[i] == 2 ? PHYSICS_WHITE : (uint16_t)0x2104);
    c.fillRect(x, y, 4, 4, color);
    if (d.alive[i] == 2) d.alive[i] = 0;
  }
  // The graph: atoms left over time. Grey: exactly half each half-life; yellow: these atoms
  const int left = 102, top = 50, bottom = 96;
  c.drawFastVLine(left, top, bottom - top + 1, PHYSICS_DARK);
  c.drawFastHLine(left, bottom, PHYSICS_DECAY_CURVE, PHYSICS_DARK);
  for (int k = 1; k <= 5; k++) c.drawFastVLine(left + k * (PHYSICS_DECAY_CURVE - 1) / 5, bottom - 2, 3, PHYSICS_GREY);
  int lastY = top;
  for (int i = 1; i < PHYSICS_DECAY_CURVE; i++) {
    const float t = span * i / (PHYSICS_DECAY_CURVE - 1);
    const int y = bottom - (int)((bottom - top) * physicsRemaining(1.0f, t, halfLife));
    c.drawLine(left + i - 1, lastY, left + i, y, PHYSICS_GREY);
    lastY = y;
  }
  for (int i = 1; i <= sample; i++) {
    const int y0 = bottom - (bottom - top) * d.curve[i - 1] / total;
    const int y1 = bottom - (bottom - top) * d.curve[i] / total;
    c.drawLine(left + i - 1, y0, left + i, y1, PHYSICS_CITRUS);
  }

  char text[28], number[16];
  physicsText(c, s.normal, s.german ? isotope.de : isotope.en, left, 18, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsFormatNumber(number, sizeof(number), isotope.halfLife, isotope.decimals, s.german);
  snprintf(text, sizeof(text), "%s %s", number, s.german ? isotope.unitDe : isotope.unitEn);
  physicsText(c, s.normal, text, left, 40, PHYSICS_CITRUS, textdatum_t::baseline_left);
  snprintf(text, sizeof(text), "%d", d.count);
  physicsText(c, s.normal, text, PHYSICS_W - 2, 66, PHYSICS_GREEN, textdatum_t::baseline_right);
  // How much time has really passed
  const float passed = physicsClamp(s.c, 0.0f, span) / halfLife * isotope.halfLife;
  physicsFormatNumber(number, sizeof(number), passed, isotope.decimals > 0 ? 1 : 0, s.german);
  snprintf(text, sizeof(text), "%s %s", number, s.german ? isotope.unitDe : isotope.unitEn);
  physicsText(c, s.normal, text, 2, 112, PHYSICS_WHITE, textdatum_t::baseline_left);
  physicsText(c, s.normal, physicsPick(s, "Zeitraffer", "time-lapse"), PHYSICS_W - 2, 112, PHYSICS_GREY,
              textdatum_t::baseline_right);
}
