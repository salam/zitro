// World clock: "Wie spät ist es in Tokio?" shows the Earth turned to that city, lit by the Sun as it is right
// now, with the time there and at home. The time is always computed here: daylight saving from the zone's
// rule, the Sun from the date. The written answer too (modelAnswerText): the AI never knows the time.
//
// The 320 big cities of clock_data.h are built in and need no network, so they also work for a shake. Any
// other place comes from zitro.spot/api/place.php (GeoNames, see scripts/make_place_data.py), which only
// says where it is and which rule its zone follows.
//
// The file comes after feature_globe.ino in the build, and reuses its coastline, shading and spin:
// only one result is loaded at a time, so they share the globe's buffers.
#include "clock_core.h"
#include "clock_place.h"

static const size_t CLOCK_PLACE_REPLY_LIMIT = 1024;

static int clockCity = -1;  // -1 and clockIsPlace true: a place from the server, or home if that's a place
static bool clockIsPlace = false;
static ClockPlace clockPlace;
static bool clockAtHome = false;  // The place shown is home: no second time, no house
static ClockRule clockHomeRule;    // Home's, whether a city of the table or a place (homeRule)
static bool clockGerman = true;
static GlobeSpin clockSpin;
static float clockPin[3];
static int16_t clockCapitalXyz[CLOCK_CITY_COUNT][3];  // Unit vectors x 2^14, filled once per lookup

static void clockUnload() {
  globeUnload();
  clockCity = -1;
  clockIsPlace = false;
  clockAtHome = false;
}

// A place that isn't built in, from the server. Only asked once the built-in table has said no. Also for a
// home named by voice.
ClockPlaceResult clockAskServer(const char* query, ClockPlace& out) {
  char name[64], hint[64];
  if (!clockSplitPlaceQuery(query, name, sizeof(name), hint, sizeof(hint))) return CLOCK_PLACE_UNKNOWN;
  String url = String(CLOCK_PLACE_URL) + "?q=" + name;  // Folded: letters a-z only, nothing to encode
  if (hint[0]) url += String("&r=") + hint;
  char* body = (char*)ps_malloc(CLOCK_PLACE_REPLY_LIMIT);
  size_t length = 0;
  const bool answered = body && eduHttpGet(url, millis() + EDU_LOOKUP_DEADLINE_MS, body, CLOCK_PLACE_REPLY_LIMIT, length);
  const ClockPlaceResult result = answered ? clockParsePlace(body, length, out) : CLOCK_PLACE_BAD;
  free(body);
  return result;
}

static bool clockFetch(const char* query) {
  clockGerman = deviceLanguageIsGerman();
  const time_t now = time(nullptr);
  const bool clockKnown = now >= (time_t)GLOBE_CLOCK_VALID;
  char folded[16];
  clockFold(query, folded, sizeof(folded));
  const bool here = strcmp(folded, "here") == 0 || strcmp(folded, "home") == 0;
  const int home = clockHomeCity();  // -1: home is a place, kept in the settings
  ClockPlace homeAsPlace;
  const bool homeIsPlace = homePlace(homeAsPlace);
  const int city = here ? home : clockFindCity(query);
  // The whole name first: "Paris, Texas" must not become the Paris in the table
  ClockPlaceResult asked = CLOCK_PLACE_UNKNOWN;
  if (here && homeIsPlace) {
    clockPlace = homeAsPlace;  // Home is known without asking
    asked = CLOCK_PLACE_FOUND;
  } else if (clockKnown && city < 0) {
    asked = clockAskServer(query, clockPlace);
  }
  const bool isPlace = city < 0 && asked == CLOCK_PLACE_FOUND;
  char text[256];
  if (!clockKnown || (city < 0 && !isPlace)) {
    if (clockKnown && asked == CLOCK_PLACE_BAD) clockNoServerAnswer(text, sizeof(text), clockGerman, query);
    else clockNoAnswer(text, sizeof(text), clockGerman, clockKnown, query);
    modelAnswerText = text;
    Serial.printf("Clock lookup '%s': %s\n", query, !clockKnown ? "no clock yet" : asked == CLOCK_PLACE_BAD ? "no answer from the server" : "place unknown");
    return false;
  }
  ClockRule cityRule;
  if (!homeRule(clockHomeRule) || (!isPlace && !clockZoneRule(CLOCK_CITIES[city].zone, cityRule))) return false;
  if (!globeLoadCoast(GLOBE_COAST_BUILTIN, GLOBE_COAST_BUILTIN_SIZE)) {
    Serial.println("Clock lookup: no memory for the coastline");
    return false;
  }
  clockCity = city;
  clockIsPlace = isPlace;
  // A place is home when it was asked for as home, or when it's the very place home is
  clockAtHome = here || (!isPlace && city == home) ||
                (isPlace && homeIsPlace && !strcmp(clockPlace.name, homeAsPlace.name) &&
                 fabsf(clockPlace.lat - homeAsPlace.lat) < 0.01f && fabsf(clockPlace.lon - homeAsPlace.lon) < 0.01f);
  float lat, lon;
  const char* name;
  if (isPlace) {
    name = clockPlace.label;
    cityRule = clockPlace.rule;
    modelLabel = clockPlace.label;
    modelSourceName = "GeoNames";  // Home found by zitro.spot too: the place is GeoNames'
    lat = clockPlace.lat;
    lon = clockPlace.lon;
  } else {
    name = clockCityName(clockCity, clockGerman);
    modelLabel = name;
    lat = CLOCK_CITIES[clockCity].lat100 / 100.0f;
    lon = CLOCK_CITIES[clockCity].lon100 / 100.0f;
  }
  clockAnswerFor(text, sizeof(text), clockGerman, name, cityRule, clockAtHome, homeName(clockGerman), clockHomeRule, (int64_t)now);
  modelAnswerText = text;
  globeUnitVector(lat, lon, clockPin);
  globeFacing(lat, lon, modelRotation.m);
  for (int i = 0; i < CLOCK_CITY_COUNT; i++) {
    float v[3];
    globeUnitVector(CLOCK_CITIES[i].lat100 / 100.0f, CLOCK_CITIES[i].lon100 / 100.0f, v);
    for (int k = 0; k < 3; k++) clockCapitalXyz[i][k] = (int16_t)lroundf(v[k] * GLOBE_Q14);
  }
  Serial.printf("Clock lookup '%s': %s, home %s\n", query, isPlace ? clockPlace.label : CLOCK_CITIES[clockCity].en, homeName(false));
  return true;
}

static bool clockBegin(M5Canvas& c) {
  if ((clockCity < 0 && !clockIsPlace) || !globeXyz || !c.getBuffer() || c.getColorDepth() != lgfx::rgb565_2Byte) return false;
  globePreparePalette(c);
  globeSpinReset(clockSpin);
  globeHomeLoad();
  return true;
}

// Nominal time zones: the 24 meridians at 7.5 + 15 n degrees. Real borders bend round countries; the
// stripes show the idea, the times on screen come from the real zones.
static void clockDrawZoneLines(M5Canvas& c, const float m[9], int cx, int cy, int radius, int bodyHeight) {
  uint16_t* buffer = (uint16_t*)c.getBuffer();
  c.drawPixel(0, 0, c.color565(0x2A, 0x4A, 0x70));
  const uint16_t raw = buffer[0];
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

// How far the city is ahead (+) or behind (-) of home: "+8 h", "-3:30 h", "="
static void clockFormatGap(char* out, size_t size, int32_t gap) {
  const int32_t a = gap < 0 ? -gap : gap;
  if (gap == 0) snprintf(out, size, "=");
  else if (a % 3600) snprintf(out, size, "%c%d:%02d h", gap < 0 ? '-' : '+', (int)(a / 3600), (int)(a % 3600 / 60));
  else snprintf(out, size, "%c%d h", gap < 0 ? '-' : '+', (int)(a / 3600));
}

// Home's name with the little house in front, as on the globe
static void clockHomeName(M5Canvas& c, const char* name, int y, int width, bool right) {
  const int indent = GLOBE_HOME_SIZE + 4;
  const int x = globePanelText(c, name, y, TFT_WHITE, width, right, indent);
  globeDrawHome(c, x - indent + GLOBE_HOME_SIZE / 2, y + c.fontHeight() / 2);
}

static void clockFrame(M5Canvas& c, float dt) {
  float turn[3];
  globeSpinStep(clockSpin, modelGyroDps, dt, turn);
  mat3Rotate(modelRotation, turn[0], turn[1], turn[2]);

  // The globe in the buffer's own axes, in the middle of the screen: there it stays put however the stick
  // is turned, and only the text turns to be read (EduFeature.levels)
  float m[9];
  modelLevelMatrix(m);
  const int cx = WIDTH / 2, cy = HEIGHT / 2;
  const int radius = (int)(GLOBE_LEVEL_RADIUS * GLOBE_ZOOMS[modelZoomStep]);
  c.fillSprite(TFT_BLACK);
  globeDrawEarth(c, m, cx, cy, radius, HEIGHT);
  clockDrawZoneLines(c, m, cx, cy, radius, HEIGHT);

  // Capitals as dots, white by day, grey by night: the shading already tells which
  const uint16_t dot = c.color565(0xF0, 0xF0, 0xF0);
  for (int i = 0; i < CLOCK_CITY_COUNT; i++) {
    if (!CLOCK_CITIES[i].capital || i == clockCity) continue;
    const int16_t* v = clockCapitalXyz[i];
    const float z = (m[6] * v[0] + m[7] * v[1] + m[8] * v[2]) / GLOBE_Q14;
    if (z < 0.05f) continue;
    const int x = cx + (int)lroundf((m[0] * v[0] + m[1] * v[1] + m[2] * v[2]) / GLOBE_Q14 * radius);
    const int y = cy - (int)lroundf((m[3] * v[0] + m[4] * v[1] + m[5] * v[2]) / GLOBE_Q14 * radius);
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) c.drawPixel(x, y, dot);
  }

  // Where the Sun is overhead, if that side faces us
  float sun[3];
  if (globeSunNow(m, sun) && sun[2] > 0.4f) {  // Not right at the rim, where it looks like it sits beside the globe
    const int sx = cx + (int)lroundf(sun[0] * radius), sy = cy - (int)lroundf(sun[1] * radius);
    if (sx >= 0 && sx < WIDTH && sy >= 0 && sy < HEIGHT) {
      c.fillCircle(sx, sy, 3, c.color565(0xFF, 0xE0, 0x60));
      c.drawCircle(sx, sy, 5, c.color565(0xFF, 0xE0, 0x60));
    }
  }

  const int64_t now = (int64_t)time(nullptr);
  ClockDate d, h;
  int32_t offset = 0, homeOffset = 0;
  bool timeKnown = true;
  if (clockIsPlace) clockRuleTime(clockPlace.rule, now, d, offset);
  else timeKnown = clockCityTime(clockCity, now, d, offset);
  const bool away = !clockAtHome;
  if (away) clockRuleTime(clockHomeRule, now, h, homeOffset);

  // The pin and, somewhere else, the house at home. From here on the sprite reads upright.
  globeDrawMarkers(c, m, clockPin, cx, cy, radius, away);
  const uint16_t citrus = c.color565(GLOBE_PIN_R, GLOBE_PIN_G, GLOBE_PIN_B);

  // The times, live. Landscape: the city on the left, home on the right. Portrait: the city above the globe,
  // home below it. Bottom lines first: each line's black box may reach into the top of the one below.
  if (timeKnown) {
    const bool portrait = c.height() > c.width();
    const int width = portrait ? c.width() - 6 : GLOBE_PANEL_W;
    char line[48], zone[16], gapText[16] = "", homeTime[8];
    c.setTextDatum(textdatum_t::top_left);
    c.setTextSize(1);
    if (away) {
      clockFormatGap(gapText, sizeof(gapText), offset - homeOffset);
      snprintf(homeTime, sizeof(homeTime), "%02d:%02d", h.hour, h.minute);
      const char* home = homeName(clockGerman);
      if (portrait) {
        c.setFreeFont(FontManager::getFont16pt());
        globePanelText(c, homeTime, 192, TFT_LIGHTGREY, width);
        c.setFreeFont(FontManager::getFont12pt());
        clockHomeName(c, home, 173, width, false);
      } else {
        c.setFreeFont(FontManager::getFont16pt());
        globePanelText(c, homeTime, 21, TFT_LIGHTGREY, width, true);
        c.setFreeFont(FontManager::getFont12pt());
        clockHomeName(c, home, 2, width, true);
      }
    }
    // Short weekday and UTC offset share a line: "So, UTC+9"
    static const char* const shortDe[7] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
    static const char* const shortEn[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    clockFormatOffset(zone, sizeof(zone), offset);
    snprintf(line, sizeof(line), "%s, %s", clockGerman ? shortDe[d.weekday] : shortEn[d.weekday], zone);
    c.setFreeFont(FontManager::getFont12pt());
    globePanelText(c, line, portrait ? 49 : 96, TFT_LIGHTGREY, width);
    if (gapText[0] && !portrait) globePanelText(c, gapText, 52, TFT_LIGHTGREY, width);
    c.setFreeFont(FontManager::getFont16pt());
    snprintf(line, sizeof(line), "%02d:%02d", d.hour, d.minute);
    globePanelText(c, line, 21, citrus, width);
    if (gapText[0] && portrait) {
      // Next to the time, where there's room
      const int x = 3 + c.textWidth(line) + 6;
      c.setFreeFont(FontManager::getFont12pt());
      c.fillRect(x - 2, 27, c.textWidth(gapText) + 4, c.fontHeight() - 2, TFT_BLACK);
      c.setTextColor(TFT_LIGHTGREY);
      c.drawString(gapText, x, 28);
    }
    // The city's name on top, so it's clear whose time this is
    c.setFreeFont(FontManager::getFont12pt());
    if (clockIsPlace) {
      // "Knoxville, TN"; if that doesn't fit, the name matters more than the region
      globePanelText(c, c.textWidth(clockPlace.label) <= width ? clockPlace.label : clockPlace.name, 2, TFT_WHITE, width);
    } else {
      globePanelText(c, clockCityName(clockCity, clockGerman), 2, TFT_WHITE, width);
    }
  }
  eduDrawFooter(c, 0);
}

const EduFeature FEATURE_CLOCK = {
    "clock", "Natural Earth",
    "UHRZEIT: Fragt der Nutzer nach der Uhrzeit oder der Zeitverschiebung ('Wie spaet ist es?', 'Wie spaet ist es in "
    "Tokio?', 'Welche Uhrzeit ist jetzt in New York?', 'Wie viel Zeitverschiebung hat Sydney?'), setze 'model' auf "
    "'clock:<englischer Name der Stadt>', fuer die eigene Uhrzeit auf 'clock:here'. Bei einem Land nimm die Hauptstadt "
    "('Wie spaet ist es in Japan?' ergibt 'clock:Tokyo'). Auch kleinere Orte gehen ('Wie spaet ist es in Knoxville?' "
    "ergibt 'clock:Knoxville'). Nennt der Nutzer Region oder Land dazu, haenge sie mit Komma an ('Paris in Texas' ergibt "
    "'clock:Paris, Texas'). Du kennst die Uhrzeit NICHT: nenne in 'answer' nie eine "
    "Uhrzeit, ein Datum oder eine Zeitverschiebung, schreibe nur 'Ich schaue auf die Weltuhr.' oder 'Let me check the "
    "world clock.'. Zitro rechnet die Zeit selbst. image_query bleibt leer.",
    true, clockFetch, clockUnload, clockBegin, clockFrame, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true};
