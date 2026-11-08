// Globe: where a place is on Earth. The pin comes from Wikipedia's coordinates, the coastlines from
// Natural Earth: a more detailed file from the Zitro server if it's there in time, otherwise the built-in one.
// No coordinates on Earth: fetch() is false and Zitro shows the picture.
#include "globe_coast_data.h"
#include "globe_core.h"
#include "globe_wiki.h"
#include "clock_core.h"  // Where the Sun is overhead, for day and night

static const size_t GLOBE_REPLY_LIMIT = 8 * 1024;
static const unsigned long GLOBE_DETAIL_MS = 4000;  // The better coast isn't worth a long wait
static const float GLOBE_ZOOMS[3] = {1.0f, 1.6f, 2.4f};
static const unsigned long GLOBE_LOG_EVERY_MS = 5000;
static const uint8_t GLOBE_PIN_R = 0xF2, GLOBE_PIN_G = 0xB7, GLOBE_PIN_B = 0x05;  // Citrus, #f2b705

static uint16_t globeLineCount = 0, globePointCount = 0;
static uint16_t* globeLengths = nullptr;
static int16_t* globeXyz = nullptr;
static float globePin[3];
static GlobeSpin globeSpin;
static uint16_t globeCoastRaw = 0;  // The coast's colour as the sprite stores it
static uint16_t globeOceanRaw[GLOBE_LIGHT_STEPS + 1];  // Ocean from night to day, as the sprite stores it
static const unsigned long GLOBE_CLOCK_VALID = 1704067200UL;  // 2024-01-01: before that the clock was never set

// Colours as the sprite stores them (byte order differs from color565), so frames can write the buffer
static void globePreparePalette(M5Canvas& c) {
  uint16_t* buffer = (uint16_t*)c.getBuffer();
  c.drawPixel(0, 0, c.color565(0xA8, 0xE0, 0xA0));
  globeCoastRaw = buffer[0];
  const int day[3] = {0x14, 0x4A, 0x8C}, night[3] = {0x01, 0x04, 0x0C};
  for (int i = 0; i <= GLOBE_LIGHT_STEPS; i++) {
    int rgb[3];
    for (int k = 0; k < 3; k++) rgb[k] = night[k] + (day[k] - night[k]) * i / GLOBE_LIGHT_STEPS;
    c.drawPixel(0, 0, c.color565(rgb[0], rgb[1], rgb[2]));
    globeOceanRaw[i] = buffer[0];
  }
}

// The Sun's direction in screen axes now. False without a clock: then it's all day.
static bool globeSunNow(const float m[9], float sun[3]) {
  const time_t now = time(nullptr);
  if (now < (time_t)GLOBE_CLOCK_VALID) return false;
  float lat, lon, earth[3];
  clockSubsolarPoint((double)now, lat, lon);
  globeUnitVector(lat, lon, earth);
  globeToScreen(m, earth, sun);
  return true;
}

// Ocean with day and night, then the coast. Shared with the world clock. Returns coast pieces drawn.
static uint32_t globeShadeUs = 0;  // Last frame's shading time, for the serial log

static uint32_t globeDrawEarth(M5Canvas& c, int cx, int cy, int radius, int bodyHeight) {
  uint16_t* buffer = (uint16_t*)c.getBuffer();
  float sun[3];
  const uint32_t shadeStart = micros();
  const bool lit = globeSunNow(modelRotation.m, sun);
  globeShadeDisc(buffer, WIDTH, bodyHeight, cx, cy, radius, lit ? sun : nullptr, globeOceanRaw);
  globeShadeUs = micros() - shadeStart;
  c.drawCircle(cx, cy, radius, c.color565(0x3A, 0x6E, 0xA8));
  int32_t q[9];
  globeRotationToQ14(modelRotation.m, q);
  const uint16_t raw = globeCoastRaw;
  return globeForEachSegment(globeLengths, globeLineCount, globeXyz, q, cx, cy, radius,
                             [=](int x0, int y0, int x1, int y1) {
                               globeRasterLine(buffer, WIDTH, bodyHeight, x0, y0, x1, y1, raw);
                             });
}

// Serial log: what a frame costs. modelViewTick caps the view at 25 per second, so the frame rate alone
// can't show how much room is left.
static unsigned long globeLogStart = 0, globeLogLastFrame = 0;
static uint32_t globeLogFrames = 0, globeLogDrawUs = 0, globeLogDrawMaxUs = 0, globeLogGapMaxUs = 0, globeLogSegments = 0;

static void globeUnload() {
  free(globeLengths);
  globeLengths = nullptr;
  free(globeXyz);
  globeXyz = nullptr;
  globeLineCount = globePointCount = 0;
}

// Turns a checked coastline file into points ready to draw. PSRAM: it's read once per frame, in order.
static bool globeLoadCoast(const uint8_t* data, size_t size) {
  uint16_t lines, points;
  if (!globeCheckFile(data, size, lines, points)) return false;
  uint16_t* lengths = (uint16_t*)ps_malloc(sizeof(uint16_t) * lines);
  int16_t* xyz = (int16_t*)ps_malloc(sizeof(int16_t) * 3 * points);
  if (!lengths || !xyz) {
    free(lengths);
    free(xyz);
    return false;
  }
  globeDecode(data, lines, points, lengths, xyz);
  globeUnload();
  globeLengths = lengths;
  globeXyz = xyz;
  globeLineCount = lines;
  globePointCount = points;
  return true;
}

// The detailed coast. False if it isn't there in time, or isn't right: the built-in one will do.
static bool globeLoadDetail(unsigned long deadline) {
  uint8_t* file = (uint8_t*)ps_malloc(GLOBE_FILE_LIMIT);
  if (!file) return false;
  unsigned long until = millis() + GLOBE_DETAIL_MS;
  if ((long)(deadline - until) < 0) until = deadline;
  size_t size = 0;
  const bool ok = eduHttpGet(EDU_DATA_BASE_URL "globe/coast_detail.bin", until, (char*)file, GLOBE_FILE_LIMIT, size) &&
                  globeLoadCoast(file, size);
  free(file);
  return ok;
}

static bool globeFetch(const char* title) {
  const unsigned long deadline = millis() + EDU_LOOKUP_DEADLINE_MS;
  char* body = (char*)ps_malloc(GLOBE_REPLY_LIMIT);
  if (!body) return false;
  const String url =
      "https://en.wikipedia.org/w/api.php?action=query&prop=coordinates%7Clanglinks&lllang=de"
      "&format=json&formatversion=2&redirects=1&titles=" + eduUrlEncode(title);
  size_t length = 0;
  GlobePlace place;
  const bool found = eduHttpGet(url, deadline, body, GLOBE_REPLY_LIMIT, length) && globeParseWikiReply(body, length, place);
  free(body);
  if (!found) {
    Serial.printf("Globe lookup '%s': not available\n", title);
    return false;
  }
  if (!globeLoadDetail(deadline) && !globeLoadCoast(GLOBE_COAST_BUILTIN, GLOBE_COAST_BUILTIN_SIZE)) {
    Serial.println("Globe lookup: no memory for the coastline");
    return false;
  }
  Preferences prefs;
  prefs.begin("settings", true);
  const bool english = prefs.getString("language", "DE") == "EN";
  prefs.end();
  modelLabel = (english || !place.titleDe[0]) ? place.title : place.titleDe;
  globeUnitVector(place.lat, place.lon, globePin);
  globeFacing(place.lat, place.lon, modelRotation.m);
  Serial.printf("Globe lookup '%s': %s at %.3f, %.3f, %u points, %ld ms left\n", title, place.title, place.lat, place.lon,
                globePointCount, (long)(deadline - millis()));
  return true;
}

static bool globeBegin(M5Canvas& c) {
  // frame() writes straight into the sprite, so it has to be the plain 16-bit one
  if (!globeXyz || !c.getBuffer() || c.getColorDepth() != lgfx::rgb565_2Byte) return false;
  globePreparePalette(c);
  globeSpinReset(globeSpin);
  globeLogStart = globeLogLastFrame = 0;
  globeLogFrames = globeLogDrawUs = globeLogDrawMaxUs = globeLogGapMaxUs = globeLogSegments = 0;
  return true;
}

static void globeDrawPin(M5Canvas& c, int x, int y, uint16_t color) {
  const int headY = y - GLOBE_PIN_HEIGHT + 6;
  c.fillTriangle(x - 5, headY + 2, x + 5, headY + 2, x, y, TFT_BLACK);
  c.fillCircle(x, headY, 6, TFT_BLACK);
  c.fillTriangle(x - 4, headY + 2, x + 4, headY + 2, x, y - 1, color);
  c.fillCircle(x, headY, 5, color);
  c.fillCircle(x, headY, 2, TFT_BLACK);
}

static void globeFrame(M5Canvas& c, float dt) {
  const uint32_t startUs = micros();
  float turn[3];
  globeSpinStep(globeSpin, modelGyroDps, dt, turn);
  mat3Rotate(modelRotation, turn[0], turn[1], turn[2]);

  const int bodyHeight = HEIGHT - EDU_FOOTER_HEIGHT;
  const int cx = WIDTH / 2, cy = bodyHeight / 2;
  const int radius = (int)(bodyHeight * 0.47f * GLOBE_ZOOMS[modelZoomStep]);
  c.fillSprite(TFT_BLACK);
  const uint32_t segments = globeDrawEarth(c, cx, cy, radius, bodyHeight);

  GlobePinView pin;
  globeLocatePin(modelRotation.m, globePin, cx, cy, radius, WIDTH, bodyHeight, pin);
  const uint16_t citrus = c.color565(GLOBE_PIN_R, GLOBE_PIN_G, GLOBE_PIN_B);
  if (pin.onScreen) {
    globeDrawPin(c, pin.x, pin.y, citrus);
  } else {
    c.fillTriangle(pin.arrow[0], pin.arrow[1], pin.arrow[2], pin.arrow[3], pin.arrow[4], pin.arrow[5], citrus);
    c.drawTriangle(pin.arrow[0], pin.arrow[1], pin.arrow[2], pin.arrow[3], pin.arrow[4], pin.arrow[5], TFT_BLACK);
  }
  eduDrawFooter(c, 0);

  const uint32_t endUs = micros();
  const uint32_t drawUs = endUs - startUs;
  globeLogDrawUs += drawUs;
  if (drawUs > globeLogDrawMaxUs) globeLogDrawMaxUs = drawUs;
  if (globeLogLastFrame && startUs - globeLogLastFrame > globeLogGapMaxUs) globeLogGapMaxUs = startUs - globeLogLastFrame;
  globeLogLastFrame = startUs;
  globeLogSegments += segments;
  globeLogFrames++;
  const unsigned long now = millis();
  if (!globeLogStart) globeLogStart = now;
  if (now - globeLogStart >= GLOBE_LOG_EVERY_MS) {
    Serial.printf("globe fps=%.1f drawUs avg=%u max=%u shadeUs=%u gapUs max=%u points=%u pieces=%u zoom=%u sprite=%s\n",
                  globeLogFrames * 1000.0f / (now - globeLogStart), (unsigned)(globeLogDrawUs / globeLogFrames),
                  (unsigned)globeLogDrawMaxUs, (unsigned)globeShadeUs, (unsigned)globeLogGapMaxUs, globePointCount,
                  (unsigned)(globeLogSegments / globeLogFrames), modelZoomStep, modelViewSpriteInPsram() ? "psram" : "internal");
    globeLogStart = now;
    globeLogFrames = globeLogDrawUs = globeLogDrawMaxUs = globeLogGapMaxUs = globeLogSegments = 0;
  }
}

const EduFeature FEATURE_GLOBE = {
    "globe", "Wikipedia",
    "GLOBUS: Setze 'model' auf 'globe:<genauer englischer Wikipedia-Artikeltitel>' NUR wenn gefragt wird, WO ein Ort "
    "auf der Erde liegt (z.B. 'Wo ist das Matterhorn?' ergibt 'globe:Matterhorn', 'Wo liegt Paris?' ergibt 'globe:Paris'). "
    "Andere Fragen zu einem Ort ('Wie sieht das Matterhorn aus?', 'Wie hoch ist das Matterhorn?') bekommen kein globe. "
    "Nenne nur den Titel, nie Koordinaten oder Zahlen.",
    true, globeFetch, globeUnload, globeBegin, globeFrame, nullptr, nullptr, nullptr, nullptr};
