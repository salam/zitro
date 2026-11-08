// World clock: "Wie spät ist es in Tokio?" shows the Earth turned to that city, lit by the Sun as it is right
// now, with the time there and at home. Everything is built in and computed: cities and zones from
// clock_data.h, daylight saving from the zones' rules, the Sun from the date. No network, so it also works
// for a shake. The written answer is computed too (modelAnswerText): the AI never knows the time.
//
// The file comes after feature_globe.ino in the build, and reuses its coastline, shading and spin:
// only one result is loaded at a time, so they share the globe's buffers.
#include "clock_core.h"

static int clockCity = -1, clockHome = -1;
static bool clockGerman = true;
static GlobeSpin clockSpin;
static float clockPin[3];
static int16_t clockCapitalXyz[CLOCK_CITY_COUNT][3];  // Unit vectors x 2^14, filled once per lookup
static const int CLOCK_GLOBE_CX = 168;                 // The globe sits right, the times left
static const uint16_t CLOCK_PANEL_W = 112;

static void clockUnload() {
  globeUnload();
  clockCity = clockHome = -1;
}

static bool clockFetch(const char* query) {
  clockGerman = deviceLanguageIsGerman();
  const time_t now = time(nullptr);
  const bool clockKnown = now >= (time_t)GLOBE_CLOCK_VALID;
  char folded[16];
  clockFold(query, folded, sizeof(folded));
  const bool here = strcmp(folded, "here") == 0 || strcmp(folded, "home") == 0;
  const int city = here ? clockHomeCity() : clockFindCity(query);
  char text[256];
  if (!clockKnown || city < 0) {
    clockNoAnswer(text, sizeof(text), clockGerman, clockKnown, query);
    modelAnswerText = text;
    Serial.printf("Clock lookup '%s': %s\n", query, clockKnown ? "city not in the table" : "no clock yet");
    return false;
  }
  if (!globeLoadCoast(GLOBE_COAST_BUILTIN, GLOBE_COAST_BUILTIN_SIZE)) {
    Serial.println("Clock lookup: no memory for the coastline");
    return false;
  }
  clockCity = city;
  clockHome = clockHomeCity();
  clockAnswer(text, sizeof(text), clockGerman, clockCity, clockHome, (int64_t)now);
  modelAnswerText = text;
  modelLabel = clockCityName(clockCity, clockGerman);
  const ClockCity& c = CLOCK_CITIES[clockCity];
  globeUnitVector(c.lat100 / 100.0f, c.lon100 / 100.0f, clockPin);
  globeFacing(c.lat100 / 100.0f, c.lon100 / 100.0f, modelRotation.m);
  for (int i = 0; i < CLOCK_CITY_COUNT; i++) {
    float v[3];
    globeUnitVector(CLOCK_CITIES[i].lat100 / 100.0f, CLOCK_CITIES[i].lon100 / 100.0f, v);
    for (int k = 0; k < 3; k++) clockCapitalXyz[i][k] = (int16_t)lroundf(v[k] * GLOBE_Q14);
  }
  Serial.printf("Clock lookup '%s': %s, %s, home %s\n", query, c.en, CLOCK_ZONES[c.zone].iana, CLOCK_CITIES[clockHome].en);
  return true;
}

static bool clockBegin(M5Canvas& c) {
  if (clockCity < 0 || !globeXyz || !c.getBuffer() || c.getColorDepth() != lgfx::rgb565_2Byte) return false;
  globePreparePalette(c);
  globeSpinReset(clockSpin);
  return true;
}

// Text on the left, on black so it stays readable when the zoomed globe runs under it
static void clockPanelText(M5Canvas& c, const char* text, int y, uint16_t color) {
  String s = text;
  if (c.textWidth(s) > CLOCK_PANEL_W) {
    while (s.length() > 1 && c.textWidth(s + "...") > CLOCK_PANEL_W) {
      int cut = s.length() - 1;
      while (cut > 0 && ((uint8_t)s[cut] & 0xC0) == 0x80) cut--;
      s.remove(cut);
    }
    s += "...";
  }
  const int w = c.textWidth(s), h = c.fontHeight();
  c.fillRect(0, y, w + 6, h, TFT_BLACK);
  c.setTextColor(color);
  c.drawString(s, 3, y);
}

// Nominal time zones: the 24 meridians at 7.5 + 15 n degrees. Real borders bend round countries; the
// stripes show the idea, the times on screen come from the real zones.
static void clockDrawZoneLines(M5Canvas& c, int cx, int cy, int radius, int bodyHeight) {
  uint16_t* buffer = (uint16_t*)c.getBuffer();
  c.drawPixel(0, 0, c.color565(0x2A, 0x4A, 0x70));
  const uint16_t raw = buffer[0];
  const float* m = modelRotation.m;
  for (int z = 0; z < 24; z++) {
    const float lon = -172.5f + 15.0f * z;
    int lastX = 0, lastY = 0;
    bool lastSeen = false;
    for (int lat = -80; lat <= 80; lat += 5) {
      float v[3], s[3];
      globeUnitVector((float)lat, lon, v);
      globeToScreen(m, v, s);
      const int x = cx + (int)lroundf(s[0] * radius), y = cy - (int)lroundf(s[1] * radius);
      const bool seen = s[2] >= 0;
      if (seen && lastSeen) globeRasterLine(buffer, WIDTH, bodyHeight, lastX, lastY, x, y, raw);
      lastX = x;
      lastY = y;
      lastSeen = seen;
    }
  }
}

static void clockFrame(M5Canvas& c, float dt) {
  float turn[3];
  globeSpinStep(clockSpin, modelGyroDps, dt, turn);
  mat3Rotate(modelRotation, turn[0], turn[1], turn[2]);

  const int bodyHeight = HEIGHT - EDU_FOOTER_HEIGHT;
  const int cx = CLOCK_GLOBE_CX, cy = bodyHeight / 2;
  const int radius = (int)(bodyHeight * 0.47f * GLOBE_ZOOMS[modelZoomStep]);
  c.fillSprite(TFT_BLACK);
  globeDrawEarth(c, cx, cy, radius, bodyHeight);
  clockDrawZoneLines(c, cx, cy, radius, bodyHeight);

  // Capitals as dots, white by day, grey by night: the shading already tells which
  const float* m = modelRotation.m;
  const uint16_t dot = c.color565(0xF0, 0xF0, 0xF0);
  for (int i = 0; i < CLOCK_CITY_COUNT; i++) {
    if (!CLOCK_CITIES[i].capital || i == clockCity) continue;
    const int16_t* v = clockCapitalXyz[i];
    const float z = (m[6] * v[0] + m[7] * v[1] + m[8] * v[2]) / GLOBE_Q14;
    if (z < 0.05f) continue;
    const int x = cx + (int)lroundf((m[0] * v[0] + m[1] * v[1] + m[2] * v[2]) / GLOBE_Q14 * radius);
    const int y = cy - (int)lroundf((m[3] * v[0] + m[4] * v[1] + m[5] * v[2]) / GLOBE_Q14 * radius);
    if (x >= 0 && x < WIDTH && y >= 0 && y < bodyHeight) c.drawPixel(x, y, dot);
  }

  // Where the Sun is overhead, if that side faces us
  float sun[3];
  if (globeSunNow(m, sun) && sun[2] > 0.4f) {  // Not right at the rim, where it looks like it sits beside the globe
    const int sx = cx + (int)lroundf(sun[0] * radius), sy = cy - (int)lroundf(sun[1] * radius);
    if (sx >= 0 && sx < WIDTH && sy >= 0 && sy < bodyHeight) {
      c.fillCircle(sx, sy, 3, c.color565(0xFF, 0xE0, 0x60));
      c.drawCircle(sx, sy, 5, c.color565(0xFF, 0xE0, 0x60));
    }
  }

  GlobePinView pin;
  globeLocatePin(m, clockPin, cx, cy, radius, WIDTH, bodyHeight, pin);
  const uint16_t citrus = c.color565(GLOBE_PIN_R, GLOBE_PIN_G, GLOBE_PIN_B);
  if (pin.onScreen) {
    globeDrawPin(c, pin.x, pin.y, citrus);
  } else {
    c.fillTriangle(pin.arrow[0], pin.arrow[1], pin.arrow[2], pin.arrow[3], pin.arrow[4], pin.arrow[5], citrus);
    c.drawTriangle(pin.arrow[0], pin.arrow[1], pin.arrow[2], pin.arrow[3], pin.arrow[4], pin.arrow[5], TFT_BLACK);
  }

  // The times, live
  const int64_t now = (int64_t)time(nullptr);
  ClockDate d, h;
  int32_t offset = 0, homeOffset = 0;
  c.setTextDatum(textdatum_t::top_left);
  c.setTextSize(1);
  if (clockCityTime(clockCity, now, d, offset)) {
    char line[48], zone[16];
    // Bottom line first: each line's black box may reach into the descenders of the one above
    c.setFreeFont(FontManager::getFont12pt());
    const bool away = clockHome != clockCity && clockCityTime(clockHome, now, h, homeOffset);
    if (away) {
      snprintf(line, sizeof(line), "%02d:%02d", h.hour, h.minute);
      clockPanelText(c, line, 96, TFT_LIGHTGREY);
      clockPanelText(c, clockCityName(clockHome, clockGerman), 75, TFT_WHITE);
    }
    clockFormatOffset(zone, sizeof(zone), offset);
    clockPanelText(c, zone, 52, TFT_LIGHTGREY);
    clockPanelText(c, clockGerman ? CLOCK_WEEKDAY_DE[d.weekday] : CLOCK_WEEKDAY_EN[d.weekday], 31, TFT_WHITE);
    char gapText[16] = "";
    if (away) {
      // How far the city is ahead (+) or behind (-) of home, next to its time
      const int32_t gap = offset - homeOffset;
      const int32_t a = gap < 0 ? -gap : gap;
      if (gap == 0) snprintf(gapText, sizeof(gapText), "=");
      else if (a % 3600) snprintf(gapText, sizeof(gapText), "%c%d:%02d h", gap < 0 ? '-' : '+', (int)(a / 3600), (int)(a % 3600 / 60));
      else snprintf(gapText, sizeof(gapText), "%c%d h", gap < 0 ? '-' : '+', (int)(a / 3600));
    }
    c.setFreeFont(FontManager::getFont16pt());
    snprintf(line, sizeof(line), "%02d:%02d", d.hour, d.minute);
    clockPanelText(c, line, 2, citrus);
    if (gapText[0]) {
      const int x = 3 + c.textWidth(line) + 6;
      c.setFreeFont(FontManager::getFont12pt());
      c.fillRect(x - 2, 8, c.textWidth(gapText) + 4, c.fontHeight() - 2, TFT_BLACK);
      c.setTextColor(TFT_LIGHTGREY);
      c.drawString(gapText, x, 9);
    }
  }
  eduDrawFooter(c, 0);
}

const EduFeature FEATURE_CLOCK = {
    "clock", "Natural Earth",
    "UHRZEIT: Fragt der Nutzer nach der Uhrzeit oder der Zeitverschiebung ('Wie spaet ist es?', 'Wie spaet ist es in "
    "Tokio?', 'Welche Uhrzeit ist jetzt in New York?', 'Wie viel Zeitverschiebung hat Sydney?'), setze 'model' auf "
    "'clock:<englischer Name der Stadt>', fuer die eigene Uhrzeit auf 'clock:here'. Bei einem Land nimm die Hauptstadt "
    "('Wie spaet ist es in Japan?' ergibt 'clock:Tokyo'). Du kennst die Uhrzeit NICHT: nenne in 'answer' nie eine "
    "Uhrzeit, ein Datum oder eine Zeitverschiebung, schreibe nur 'Ich schaue auf die Weltuhr.' oder 'Let me check the "
    "world clock.'. Zitro rechnet die Zeit selbst. image_query bleibt leer.",
    true, clockFetch, clockUnload, clockBegin, clockFrame, nullptr, nullptr, nullptr, nullptr};
