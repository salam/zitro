// Live ISS: "Wo ist die ISS gerade?" shows the Earth lit as it is now, with the space station where it is
// this second and its track. zitro.spot/api/iss.php computes the track from NASA's published ephemeris
// (see webpage2/api/iss_lib.php) for 45 minutes back to 3 hours ahead; the stick moves the ISS along it
// by the clock. The written answer is computed from the track too: the AI never knows where the ISS is.
//
// Comes after feature_globe.ino in the build and reuses its coastline, day and night, spin and panel.
#include "iss_core.h"

static const char ISS_URL[] = "https://zitro.spot/api/iss.php";
static const size_t ISS_REPLY_LIMIT = 16 * 1024;
static const int ISS_PAST_S = 45 * 60, ISS_AHEAD_S = 90 * 60;  // Track drawn behind and ahead
static const int ISS_PANEL_W = 122;

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
  issAnswer(text, sizeof(text), issGerman, lat, lon, alt, issTrack->speedKmh, issTrack->periodMin);
  modelAnswerText = text;
  modelLabel = issGerman ? "ISS live" : "ISS live";
  globeFacing(lat, lon, modelRotation.m);
  Serial.printf("ISS lookup: %.2f, %.2f, %.0f km, %d points from %lld\n", lat, lon, alt, issTrack->count,
                (long long)issTrack->t0);
  return true;
}

static bool issBegin(M5Canvas& c) {
  if (!issTrack || !globeXyz || !c.getBuffer() || c.getColorDepth() != lgfx::rgb565_2Byte) return false;
  globePreparePalette(c);
  globeSpinReset(issSpin);
  return true;
}

// The track from t0 to t1 as a line on the near side of the globe
static void issDrawTrack(M5Canvas& c, double t0, double t1, int cx, int cy, int radius, int bodyHeight, uint16_t color) {
  const float* m = modelRotation.m;
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

  const int bodyHeight = HEIGHT - EDU_FOOTER_HEIGHT;
  const int cx = 180, cy = bodyHeight / 2;  // A little further right than the clock: the lines are longer
  const int radius = (int)(bodyHeight * 0.47f * GLOBE_ZOOMS[modelZoomStep]);
  c.fillSprite(TFT_BLACK);
  globeDrawEarth(c, cx, cy, radius, bodyHeight);

  const double now = issNow();
  issDrawTrack(c, now - ISS_PAST_S, now, cx, cy, radius, bodyHeight, c.color565(0x80, 0x80, 0x80));
  issDrawTrack(c, now, now + ISS_AHEAD_S, cx, cy, radius, bodyHeight, c.color565(GLOBE_PIN_R, GLOBE_PIN_G, GLOBE_PIN_B));

  float lat = 0, lon = 0, alt = 0;
  const bool known = issPositionAt(*issTrack, now, lat, lon, alt);
  c.setTextDatum(textdatum_t::top_left);
  c.setTextSize(1);
  c.setFreeFont(FontManager::getFont12pt());
  if (known) {
    float v[3];
    globeUnitVector(lat, lon, v);
    GlobePinView where;
    globeLocatePin(modelRotation.m, v, cx, cy, radius, WIDTH, bodyHeight, where);
    const uint16_t citrus = c.color565(GLOBE_PIN_R, GLOBE_PIN_G, GLOBE_PIN_B);
    if (where.onScreen) {
      issDrawStation(c, where.x, where.y);
    } else {
      c.fillTriangle(where.arrow[0], where.arrow[1], where.arrow[2], where.arrow[3], where.arrow[4], where.arrow[5], citrus);
      c.drawTriangle(where.arrow[0], where.arrow[1], where.arrow[2], where.arrow[3], where.arrow[4], where.arrow[5], TFT_BLACK);
    }
    // Bottom line first, as in the clock: each box may reach into the line above
    char line[32], number[16];
    issFormatCoordinate(line, sizeof(line), lon, false, issGerman);
    globePanelText(c, line, 96, TFT_LIGHTGREY, ISS_PANEL_W);
    issFormatCoordinate(line, sizeof(line), lat, true, issGerman);
    globePanelText(c, line, 77, TFT_LIGHTGREY, ISS_PANEL_W);
    issFormatThousands(number, sizeof(number), issTrack->speedKmh);
    snprintf(line, sizeof(line), "%s km/h", number);
    globePanelText(c, line, 52, TFT_WHITE, ISS_PANEL_W);
    snprintf(line, sizeof(line), issGerman ? "%d km hoch" : "%d km up", (int)lroundf(alt));
    globePanelText(c, line, 33, TFT_WHITE, ISS_PANEL_W);
  } else {
    // Past the end of the track: no position is better than a wrong one
    globePanelText(c, issGerman ? "Frag nochmals," : "Ask again,", 33, TFT_WHITE, ISS_PANEL_W);
    globePanelText(c, issGerman ? "die Bahn ist alt." : "the track is old.", 52, TFT_WHITE, ISS_PANEL_W);
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
    true, issFetch, issUnload, issBegin, issFrame, nullptr, nullptr, nullptr, nullptr};
