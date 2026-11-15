// Live ISS: "Wo ist die ISS gerade?" shows the Earth lit as it is now, with the space station where it is
// this second and its track. zitro.spot/api/iss.php computes the track from NASA's published ephemeris
// (see webpage2/api/iss_lib.php) for 45 minutes back to 3 hours ahead; the stick moves the ISS along it
// by the clock. The written answer is computed from the track too: the AI never knows where the ISS is.
//
// Comes after feature_globe.ino in the build and reuses its coastline, day and night, spin and panel.
#include "iss_core.h"

// places=1: also the place under the ISS along the track (Natural Earth, see docs/features/iss.md)
static const char ISS_URL[] = "https://zitro.spot/api/iss.php?places=1";
static const size_t ISS_REPLY_LIMIT = 24 * 1024;  // Measured at most 9.1 KB with places (tests/iss/test_iss.php)
static const int ISS_PAST_S = 45 * 60, ISS_AHEAD_S = 90 * 60;  // Track drawn behind and ahead
// The place line, above the latitude: a citrus arrow pointing down, then the name. M5GFX puts the 10 pt
// baseline 18 px under y, capitals from y + 5 to y + 19, descenders to y + 23.
static const int ISS_Y_PLACE_LANDSCAPE = 77, ISS_Y_PLACE_PORTRAIT = 154;  // Portrait: over the globe's bottom 13 rows at most
static const int ISS_PLACE_X = 13, ISS_PLACE_BOX_H = 21;
static const int ISS_TICKER_PX_S = 30, ISS_TICKER_PAUSE_MS = 1500, ISS_TICKER_GAP = 24;

static IssTrack* issTrack = nullptr;
static bool issGerman = true;
static GlobeSpin issSpin;

static void issUnload() {
  globeUnload();
  free(issTrack);
  issTrack = nullptr;
}

// The time now with the fraction of a second, so the station glides instead of jumping once a second
static double issNow() {
  static time_t lastSecond = 0;
  static unsigned long lastMillis = 0;
  const time_t s = time(nullptr);
  if (s != lastSecond) {
    lastSecond = s;
    lastMillis = millis();
  }
  return (double)s + min(999UL, millis() - lastMillis) / 1000.0;
}

static bool issFetch(const char* query) {
  (void)query;  // Only "now" exists
  issGerman = deviceLanguageIsGerman();
  char text[256];
  const time_t now = time(nullptr);
  const bool clockKnown = now >= (time_t)GLOBE_CLOCK_VALID;
  IssTrack* track = clockKnown ? (IssTrack*)ps_malloc(sizeof(IssTrack)) : nullptr;
  char* body = track ? (char*)ps_malloc(ISS_REPLY_LIMIT) : nullptr;
  size_t length = 0;
  float lat, lon, alt;
  const bool ok = body && eduHttpGet(ISS_URL, millis() + EDU_LOOKUP_DEADLINE_MS, body, ISS_REPLY_LIMIT, length) &&
                  issParseTrack(body, length, *track) && issPositionAt(*track, (double)now, lat, lon, alt);
  free(body);
  if (!ok || !globeLoadCoast(GLOBE_COAST_BUILTIN, GLOBE_COAST_BUILTIN_SIZE)) {
    free(track);
    issNoAnswer(text, sizeof(text), issGerman, clockKnown);
    modelAnswerText = text;
    Serial.printf("ISS lookup: not available (clock %s, %u bytes)\n", clockKnown ? "set" : "unknown", (unsigned)length);
    return false;
  }
  free(issTrack);
  issTrack = track;
  issAnswer(text, sizeof(text), issGerman, lat, lon, alt, issTrack->speedKmh, issTrack->periodMin,
            issPlaceAt(*issTrack, (double)now, issGerman));
  modelAnswerText = text;
  modelLabel = issGerman ? "ISS live" : "ISS live";
  modelSourceName = issTrack->placeCount > 0 ? "NASA, Natural Earth" : "NASA";
  globeFacing(lat, lon, modelRotation.m);
  Serial.printf("ISS lookup: %.2f, %.2f, %.0f km, %d points from %lld, %d places, %d changes, %u bytes\n", lat, lon,
                alt, issTrack->count, (long long)issTrack->t0, issTrack->placeCount, issTrack->placeChanges,
                (unsigned)length);
  return true;
}

static bool issBegin(M5Canvas& c) {
  if (!issTrack || !globeXyz || !c.getBuffer() || c.getColorDepth() != lgfx::rgb565_2Byte) return false;
  globePreparePalette(c);
  globeSpinReset(issSpin);
  globeHomeLoad();
  // Back from the text: the station in the middle again, where it is now. Past the end of the track
  // there's no position, so the globe stays as it was.
  float lat, lon, alt;
  if (issPositionAt(*issTrack, issNow(), lat, lon, alt)) globeFacing(lat, lon, modelRotation.m);
  return true;
}

// The track from t0 to t1 as a line on the near side of the globe
static void issDrawTrack(M5Canvas& c, const float m[9], double t0, double t1, int cx, int cy, int radius, int bodyHeight,
                         uint16_t color) {
  bool lastSeen = false;
  int lastX = 0, lastY = 0;
  for (double t = t0; t <= t1; t += 30) {
    float lat, lon, alt, v[3], s[3];
    if (!issPositionAt(*issTrack, t, lat, lon, alt)) {
      lastSeen = false;
      continue;
    }
    globeUnitVector(lat, lon, v);
    globeToScreen(m, v, s);
    const int x = cx + (int)lroundf(s[0] * radius), y = cy - (int)lroundf(s[1] * radius);
    const bool seen = s[2] >= 0;
    if (seen && lastSeen && y < bodyHeight && lastY < bodyHeight) c.drawLine(lastX, lastY, x, y, color);
    lastX = x;
    lastY = y;
    lastSeen = seen;
  }
}

// The place under the ISS: a citrus arrow pointing down, then the name as the source spells it, in white on a
// black box over the globe. A name too long for its width scrolls slowly, after a pause at its start.
static void issDrawPlace(M5Canvas& c, const char* name, int y, int width) {
  static const char* shown = nullptr;
  static unsigned long since = 0;
  if (name != shown) {
    shown = name;
    since = millis();
  }
  const int room = width - (ISS_PLACE_X - 3), textWidth = c.textWidth(name);
  c.fillRect(0, y, ISS_PLACE_X + min(textWidth, room) + 3, ISS_PLACE_BOX_H, TFT_BLACK);
  c.fillTriangle(3, y + 8, 9, y + 8, 6, y + 15, c.color565(GLOBE_PIN_R, GLOBE_PIN_G, GLOBE_PIN_B));
  c.setTextColor(TFT_WHITE);
  if (textWidth <= room) {
    c.drawString(name, ISS_PLACE_X, y);
    return;
  }
  const int period = textWidth + ISS_TICKER_GAP;
  const unsigned long cycle = ISS_TICKER_PAUSE_MS + (unsigned long)period * 1000 / ISS_TICKER_PX_S;
  const unsigned long t = (millis() - since) % cycle;
  const int offset = t < (unsigned long)ISS_TICKER_PAUSE_MS ? 0 : (int)((t - ISS_TICKER_PAUSE_MS) * ISS_TICKER_PX_S / 1000);
  c.setClipRect(ISS_PLACE_X, y, room, ISS_PLACE_BOX_H + 3);
  c.drawString(name, ISS_PLACE_X - offset, y);
  c.drawString(name, ISS_PLACE_X - offset + period, y);
  c.clearClipRect();
}

// A tiny space station: body and two solar wings
static void issDrawStation(M5Canvas& c, int x, int y) {
  const uint16_t wing = c.color565(0x60, 0x90, 0xE0);
  c.fillRect(x - 9, y - 3, 6, 7, TFT_BLACK);
  c.fillRect(x + 4, y - 3, 6, 7, TFT_BLACK);
  c.fillRect(x - 8, y - 2, 4, 5, wing);
  c.fillRect(x + 5, y - 2, 4, 5, wing);
  c.drawFastHLine(x - 4, y, 9, TFT_WHITE);
  c.fillRect(x - 1, y - 2, 3, 5, TFT_WHITE);
}

static void issFrame(M5Canvas& c, float dt) {
  float turn[3];
  globeSpinStep(issSpin, modelGyroDps, dt, turn);
  mat3Rotate(modelRotation, turn[0], turn[1], turn[2]);

  // In the middle of the screen, as the world clock: the globe stays put, the text turns (EduFeature.levels)
  float m[9];
  modelLevelMatrix(m);
  const int cx = WIDTH / 2, cy = HEIGHT / 2;
  const int radius = (int)(GLOBE_LEVEL_RADIUS * GLOBE_ZOOMS[modelZoomStep]);
  c.fillSprite(TFT_BLACK);
  globeDrawEarth(c, m, cx, cy, radius, HEIGHT);

  const double now = issNow();
  issDrawTrack(c, m, now - ISS_PAST_S, now, cx, cy, radius, HEIGHT, c.color565(0x80, 0x80, 0x80));
  issDrawTrack(c, m, now, now + ISS_AHEAD_S, cx, cy, radius, HEIGHT, c.color565(GLOBE_PIN_R, GLOBE_PIN_G, GLOBE_PIN_B));

  float lat = 0, lon = 0, alt = 0;
  const bool known = issPositionAt(*issTrack, now, lat, lon, alt);
  if (known) {
    float v[3];
    globeUnitVector(lat, lon, v);
    globeDrawMarkers(c, m, v, cx, cy, radius, true, issDrawStation);
  } else {
    c.setRotation(modelReadingTurn);
    globeDrawHomeOnGlobe(c, m, cx, cy, radius);  // Only home: no station, and no arrow
  }

  // From here the sprite reads upright. Landscape: what and how fast in the top corners, where in the bottom
  // ones. Portrait: above and below the globe. Bottom lines first, as in the clock.
  const bool portrait = c.height() > c.width();
  const int width = portrait ? c.width() - 6 : GLOBE_PANEL_W;
  c.setTextDatum(textdatum_t::top_left);
  c.setTextSize(1);
  c.setFreeFont(FontManager::getFont12pt());
  if (known) {
    char line[32], number[16];
    issFormatCoordinate(line, sizeof(line), lon, false, issGerman);
    if (portrait) globePanelText(c, line, 194, TFT_LIGHTGREY, width);
    else globePanelText(c, line, 96, TFT_LIGHTGREY, width, true);
    issFormatCoordinate(line, sizeof(line), lat, true, issGerman);
    globePanelText(c, line, portrait ? 173 : 96, TFT_LIGHTGREY, width);
    // What's below, over the latitude. No name there: no line, never a guess.
    const char* place = issPlaceAt(*issTrack, now, issGerman);
    if (place) issDrawPlace(c, place, portrait ? ISS_Y_PLACE_PORTRAIT : ISS_Y_PLACE_LANDSCAPE, width);
    issFormatThousands(number, sizeof(number), issTrack->speedKmh);
    snprintf(line, sizeof(line), "%s km/h", number);
    globePanelText(c, line, portrait ? 50 : 21, TFT_WHITE, width, !portrait);
    snprintf(line, sizeof(line), issGerman ? "%d km hoch" : "%d km up", (int)lroundf(alt));
    globePanelText(c, line, portrait ? 33 : 2, TFT_WHITE, width, !portrait);
  } else {
    // Past the end of the track: no position is better than a wrong one
    globePanelText(c, issGerman ? "die Bahn ist alt." : "the track is old.", portrait ? 50 : 96, TFT_WHITE, width);
    globePanelText(c, issGerman ? "Frag nochmals," : "Ask again,", portrait ? 33 : 77, TFT_WHITE, width);
  }
  c.setFreeFont(FontManager::getFont16pt());
  globePanelText(c, "ISS", 2, c.color565(GLOBE_PIN_R, GLOBE_PIN_G, GLOBE_PIN_B));
  eduDrawFooter(c, 0);
}

const EduFeature FEATURE_ISS = {
    "iss", "NASA",
    "ISS LIVE: Fragt der Nutzer, wo die ISS oder die Raumstation gerade ist oder jetzt fliegt ('Wo ist die ISS "
    "gerade?', 'Wo fliegt die Raumstation jetzt?', 'Where is the ISS?'), setze 'model' auf 'iss:now'. Du kennst die "
    "Position nicht: nenne in 'answer' nie Ort, Hoehe oder Geschwindigkeit, schreibe nur 'Ich schaue, wo die ISS "
    "gerade ist.' oder 'Let me look where the ISS is.'. Allgemeine Fragen zu Satellitenbahnen ('Was ist ein "
    "geostationaerer Satellit?') bleiben bei PHYSIK satellites. image_query bleibt leer.",
    true, issFetch, issUnload, issBegin, issFrame, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true};
