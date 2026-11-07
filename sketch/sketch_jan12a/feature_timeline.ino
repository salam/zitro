// Timelines: a band along a time axis with the year big, a short label and a picture.
// Dates and names come from Wikidata, pictures from a checked list. All of it is prepared by
// scripts/make_timeline_data.py and fetched as two files. See edu_feature.h and docs/features/timeline.md.
#include "timeline_core.h"

static const unsigned long TIMELINE_TOTAL_MS = 30000;  // This feature's limit instead of EDU_LOOKUP_DEADLINE_MS
static const unsigned long TIMELINE_INDEX_MS = 12000;  // The index has to work; pictures get the rest
static const int TIMELINE_SLOTS = 5;                   // Pictures kept decoded: this one and two each side
static const size_t TIMELINE_PSRAM_SPARE = 64 * 1024;  // Left free after the download
static const unsigned long TIMELINE_LOG_MS = 5000;
static const int TIMELINE_LINE_SIZE = TIMELINE_LABEL_LIMIT + 4;
static const int TIMELINE_AXIS_Y = 2;
static const int TIMELINE_IMAGE_Y = 5;
static const int TIMELINE_LABEL_Y = TIMELINE_IMAGE_Y + TIMELINE_IMAGE_H;
static const int TIMELINE_LINE_H = 17;  // The font reaches about 14 above and 4 below its baseline
static const int TIMELINE_YEAR_LARGE_H = 31;  // Digits of FreeSansBold18pt, with a margin
static const int TIMELINE_YEAR_SMALL_H = 23;  // The same for FreeSansBold12pt
static const int TIMELINE_LABEL_MAX_W = WIDTH - 8;
static const int TIMELINE_GAP_TEXT_MIN = 110;  // Below this the time between two events is too short to write in
static const int TIMELINE_COUNTER_W = 34;
// Tip the right side down (clockwise, seen from the screen) for the next event. The axis through the
// screen is the one axis that reads the same in both landscape positions. NOT tried on the stick
// yet: if tipping right goes back, flip this sign.
static const float TIMELINE_TILT_SIGN = -1.0f;

struct TimelineSlot {
  M5Canvas* canvas;
  int16_t event;
  uint32_t lastUse;
};

static Timeline* timelineData = nullptr;
static char* timelineText = nullptr;
static uint8_t* timelineBundle = nullptr;
static char* timelineLines = nullptr;  // Two lines per event, wrapped when the view opens
static TimelineSlot timelineSlots[TIMELINE_SLOTS];
static int timelineSlotCount = 0;
static uint32_t timelineUseCounter = 0;
static bool timelineEnglish = false;
static float timelinePosition = 0;
static float timelineTarget = 0;
static int timelineCurrent = 0;
static int timelineFooterEvent = -1;
static TimelineTilt timelineTilt = {0, 0, 0};
// Frame time log
static unsigned long timelineLogStart = 0, timelineLastFrameStart = 0;
static uint32_t timelineFrames = 0, timelineDrawUsMax = 0, timelineDecodes = 0, timelineDecodeUsMax = 0;
static uint64_t timelineDrawUsSum = 0, timelinePeriodUsSum = 0, timelineDecodeUsSum = 0;

static void timelineReadLanguage() {
  Preferences settings;
  settings.begin("settings", true);
  timelineEnglish = settings.getString("language", "DE") == "EN";
  settings.end();
}

static const char* timelineTitle() {
  return timelineEnglish ? timelineData->titleEn : timelineData->titleDe;
}

static void timelineUnload() {
  free(timelineData);
  timelineData = nullptr;
  free(timelineText);
  timelineText = nullptr;
  free(timelineBundle);
  timelineBundle = nullptr;
}

// Loads one timeline. The list decides first, so what the AI wrote never reaches a URL.
// Only the index has to arrive: without pictures the timeline still works.
static bool timelineFetch(const char* query) {
  timelineUnload();
  if (!timelineIsTopic(query)) {
    Serial.printf("Timeline '%s': not on the list\n", query);
    return false;
  }
  const unsigned long start = millis();
  const unsigned long deadline = start + TIMELINE_TOTAL_MS;
  timelineData = (Timeline*)ps_malloc(sizeof(Timeline));
  timelineText = (char*)ps_malloc(TIMELINE_TEXT_LIMIT + 1);
  if (!timelineData || !timelineText) {
    timelineUnload();
    return false;
  }
  const String base = String(EDU_DATA_BASE_URL "timeline/") + query;
  size_t length = 0;
  if (!eduHttpGet(base + ".txt", start + TIMELINE_INDEX_MS, timelineText, TIMELINE_TEXT_LIMIT, length) ||
      !timelineParse(timelineText, length, TIMELINE_TEXT_LIMIT + 1, query, *timelineData)) {
    Serial.printf("Timeline '%s': index not available\n", query);
    timelineUnload();
    return false;
  }
  int pictures = 0;
  size_t received = 0;
  const size_t wanted = timelineData->bundleSize;
  if (wanted > 0 && ESP.getMaxAllocPsram() >= wanted + TIMELINE_PSRAM_SPARE) {
    timelineBundle = (uint8_t*)ps_malloc(wanted);
  }
  if (timelineBundle) {
    // If it stalls, the pictures that arrived whole still count, so the result doesn't matter here
    eduHttpGet(base + ".jpgs", deadline, (char*)timelineBundle, wanted, received);
  }
  pictures = timelineCheckImages(*timelineData, timelineBundle, received);
  if (pictures == 0 && timelineBundle) {
    free(timelineBundle);
    timelineBundle = nullptr;
  }
  timelineReadLanguage();
  modelLabel = timelineTitle();
  modelSourceName = "Wikidata";
  timelineCurrent = 0;
  timelinePosition = timelineTarget = 0;
  Serial.printf("Timeline '%s': %d events, %d pictures, %u of %u bytes in %lu ms, free PSRAM %u\n", query,
                timelineData->eventCount, pictures, (unsigned)received, (unsigned)wanted, millis() - start,
                (unsigned)ESP.getFreePsram());
  return true;
}

static int timelineMeasure(const char* text, void* context) {
  return ((M5Canvas*)context)->textWidth(text);
}

static char* timelineLine(int event, int line) {
  return timelineLines + (event * 2 + line) * TIMELINE_LINE_SIZE;
}

static void timelineFreeSlots() {
  for (int i = 0; i < timelineSlotCount; i++) {
    timelineSlots[i].canvas->deleteSprite();
    delete timelineSlots[i].canvas;
    timelineSlots[i].canvas = nullptr;
  }
  timelineSlotCount = 0;
}

static void timelineEnd() {
  timelineFreeSlots();
  free(timelineLines);
  timelineLines = nullptr;
  if (timelineData) {
    modelLabel = timelineTitle();
    modelSourceName = "Wikidata";
  }
  timelineFooterEvent = -1;
}

static void timelineResetLog() {
  timelineLogStart = millis();
  timelineLastFrameStart = 0;
  timelineFrames = timelineDrawUsMax = timelineDecodes = timelineDecodeUsMax = 0;
  timelineDrawUsSum = timelinePeriodUsSum = timelineDecodeUsSum = 0;
}

static bool timelineBegin(M5Canvas& c) {
  if (!timelineData) return false;
  timelineReadLanguage();
  timelineLines = (char*)ps_malloc(TIMELINE_MAX_EVENTS * 2 * TIMELINE_LINE_SIZE);
  if (!timelineLines) return false;
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextSize(1);
  int pictures = 0;
  for (int i = 0; i < timelineData->eventCount; i++) {
    const TimelineEvent& e = timelineData->events[i];
    timelineWrapLabel(timelineEnglish ? e.labelEn : e.labelDe, TIMELINE_LABEL_MAX_W, timelineMeasure, &c,
                      timelineLine(i, 0), timelineLine(i, 1), TIMELINE_LINE_SIZE);
    if (e.hasImage) pictures++;
  }
  // Without these the pictures are missing, not the timeline
  const int wantedSlots = min(pictures, TIMELINE_SLOTS);
  for (int i = 0; i < wantedSlots; i++) {
    M5Canvas* canvas = new M5Canvas();
    canvas->setPsram(true);
    if (!canvas->createSprite(TIMELINE_IMAGE_W, TIMELINE_IMAGE_H)) {
      delete canvas;
      Serial.println("Timeline: no memory for more decoded pictures");
      break;
    }
    timelineSlots[timelineSlotCount++] = {canvas, -1, 0};
  }
  timelineFooterEvent = -1;
  timelineTilt = {0, 0, 0};
  timelineResetLog();
  return true;
}

static int timelineFindSlot(int event) {
  for (int i = 0; i < timelineSlotCount; i++) {
    if (timelineSlots[i].event == event) return i;
  }
  return -1;
}

// Decodes one picture into the least recently used slot. Costs a noticeable moment, so at most one per frame.
static void timelineDecode(int event) {
  int slot = 0;
  for (int i = 1; i < timelineSlotCount; i++) {
    if (timelineSlots[i].lastUse < timelineSlots[slot].lastUse) slot = i;
  }
  TimelineEvent& e = timelineData->events[event];
  const unsigned long start = micros();
  M5Canvas& canvas = *timelineSlots[slot].canvas;
  canvas.fillSprite(TFT_BLACK);
  const bool ok = canvas.drawJpg(timelineBundle + e.imageOffset, e.imageLength, (TIMELINE_IMAGE_W - e.imageW) / 2,
                                 (TIMELINE_IMAGE_H - e.imageH) / 2);
  const uint32_t us = micros() - start;
  timelineDecodes++;
  timelineDecodeUsSum += us;
  if (us > timelineDecodeUsMax) timelineDecodeUsMax = us;
  if (ok) {
    timelineSlots[slot].event = (int16_t)event;
    timelineSlots[slot].lastUse = ++timelineUseCounter;
  } else {
    timelineSlots[slot].event = -1;
    e.hasImage = false;  // Don't try this one again in every frame
    Serial.printf("Timeline: can't decode the picture of %s\n", e.qid);
  }
}

// Picks the next picture to decode: the one being moved to first; at rest also its neighbours.
static void timelineDecodeNext() {
  if (timelineSlotCount == 0 || !timelineBundle) return;
  const bool resting = timelinePosition == timelineTarget;
  static const int offsets[] = {0, 1, -1, 2, -2};
  const int candidates = resting ? min(timelineSlotCount, 5) : 1;
  for (int i = 0; i < candidates; i++) {
    const int event = timelineCurrent + offsets[i];
    if (event < 0 || event >= timelineData->eventCount || !timelineData->events[event].hasImage) continue;
    if (timelineFindSlot(event) >= 0) continue;
    timelineDecode(event);
    return;
  }
}

static void timelineGo(int event) {
  if (event < 0 || event >= timelineData->eventCount) return;
  timelineCurrent = event;
  timelineTarget = timelineData->events[event].bandX;
}

static void timelineDrawCard(M5Canvas& c, int event) {
  const TimelineEvent& e = timelineData->events[event];
  const int x = (int)floorf(e.bandX - timelinePosition + 0.5f);
  if (x <= -WIDTH || x >= WIDTH) return;
  const int slot = e.hasImage ? timelineFindSlot(event) : -1;
  if (slot >= 0) {
    timelineSlots[slot].canvas->pushSprite(&c, x, TIMELINE_IMAGE_Y);
    timelineSlots[slot].lastUse = ++timelineUseCounter;
  } else if (e.hasImage) {
    c.fillRect(x, TIMELINE_IMAGE_Y, WIDTH, TIMELINE_IMAGE_H, c.color565(40, 40, 40));  // Comes in a moment
  }
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::baseline_center);
  c.setTextColor(TFT_WHITE);
  const char* first = timelineLine(event, 0);
  const char* second = timelineLine(event, 1);
  const int baseline = TIMELINE_LABEL_Y + 12;  // Letters that go below it end just above the bottom line
  if (second[0]) {
    c.fillRect(x, TIMELINE_LABEL_Y - TIMELINE_LINE_H, WIDTH, TIMELINE_LINE_H, TFT_BLACK);
    c.drawString(first, x + WIDTH / 2, baseline - TIMELINE_LINE_H);
    c.drawString(second, x + WIDTH / 2, baseline);
  } else {
    c.drawString(first, x + WIDTH / 2, baseline);
  }
}

// The axis along the top, its year marks and the events on it, and the time written into a long gap
static void timelineDrawAxis(M5Canvas& c) {
  const Timeline& t = *timelineData;
  const uint16_t gold = c.color565(0xF2, 0xB7, 0x05);
  const uint16_t grey = c.color565(140, 140, 140);
  const float left = timelinePosition - WIDTH / 2;
  c.drawFastHLine(0, TIMELINE_AXIS_Y, WIDTH, grey);
  for (int i = 0; i < t.eventCount; i++) {
    const TimelineEvent& a = t.events[i];
    const float ax = a.bandX - left;
    if (i + 1 < t.eventCount) {
      const TimelineEvent& b = t.events[i + 1];
      const float bx = b.bandX - left;
      if (bx >= 0 && ax <= WIDTH) {
        const double span = (double)b.year - (double)a.year;
        const float middle = (ax + bx) / 2;
        if (a.breakAfter) {
          // Longer than it looks: the axis is cut here
          c.fillRect((int)middle - 5, 0, 10, TIMELINE_IMAGE_Y, TFT_BLACK);
          c.drawLine((int)middle - 5, 4, (int)middle - 1, 0, TFT_WHITE);
          c.drawLine((int)middle + 1, 4, (int)middle + 5, 0, TFT_WHITE);
        } else if (span > 0) {
          // Year marks only across the gap: that's the stretch where distance means time
          const float pixelsPerYear = timelineGapPxPerYear(t, i);
          const float gapLeft = ax + TIMELINE_CARD_SPACE / 2;
          const double step = t.tickYears;
          double year = ceil((double)a.year / step) * step;
          if (gapLeft < 0) year = ceil(((double)a.year - gapLeft / pixelsPerYear) / step) * step;
          for (int guard = 0; guard < 64 && year < b.year; guard++, year += step) {
            const float x = gapLeft + (float)((year - a.year) * pixelsPerYear);
            if (x > WIDTH) break;
            c.drawFastVLine((int)x, TIMELINE_AXIS_Y - 2, 5, grey);
          }
        }
        if (bx - ax - TIMELINE_CARD_SPACE >= TIMELINE_GAP_TEXT_MIN && middle > -WIDTH / 2 && middle < WIDTH * 3 / 2) {
          char text[32];
          timelineFormatSpan(a.year, b.year, timelineEnglish, text, sizeof(text));
          c.setFreeFont(FontManager::getFont12pt());
          c.setTextSize(1);
          c.setTextDatum(textdatum_t::baseline_center);
          c.setTextColor(TFT_LIGHTGREY);
          c.drawString(text, (int)middle, TIMELINE_IMAGE_Y + TIMELINE_IMAGE_H / 2 + 5);
        }
      }
    }
    if (ax >= -4 && ax <= WIDTH + 4) c.fillCircle((int)ax, TIMELINE_AXIS_Y, 3, gold);
  }
}

// The year stays in its place and runs while the band moves: that's the feeling for time
static void timelineDrawYear(M5Canvas& c) {
  int exact = -1;
  const int32_t year = timelineYearAt(*timelineData, timelinePosition, exact);
  char big[40], small[24];
  if (exact >= 0) {
    const TimelineEvent& e = timelineData->events[exact];
    timelineFormatYear(e.year, e.endYear, e.circa, e.born, timelineEnglish, big, sizeof(big), small, sizeof(small));
  } else {
    timelineFormatYear(year, 0, false, false, timelineEnglish, big, sizeof(big), small, sizeof(small));
  }
  c.setTextSize(1);
  c.setFreeFont(FontManager::getFont12pt());
  const int smallWidth = small[0] ? c.textWidth(small) + 5 : 0;
  c.setFont(&fonts::FreeSansBold18pt7b);
  bool large = true;
  if (c.textWidth(big) + smallWidth > WIDTH - 12) {
    c.setFont(&fonts::FreeSansBold12pt7b);
    large = false;
  }
  const int bigWidth = c.textWidth(big);
  const int height = large ? TIMELINE_YEAR_LARGE_H : TIMELINE_YEAR_SMALL_H;
  const int baseline = TIMELINE_IMAGE_Y + height - 4;
  c.fillRoundRect(2, TIMELINE_IMAGE_Y, bigWidth + smallWidth + 8, height, 4, TFT_BLACK);
  c.setTextDatum(textdatum_t::baseline_left);
  c.setTextColor(c.color565(0xF2, 0xB7, 0x05));
  c.drawString(big, 6, baseline);
  if (small[0]) {
    c.setFreeFont(FontManager::getFont12pt());
    c.setTextColor(TFT_WHITE);
    c.drawString(small, 6 + bigWidth + 5, baseline);
  }
}

// The bottom line names the maker of this picture where the licence asks for that; otherwise the timeline
static void timelineDrawFooter(M5Canvas& c) {
  if (timelineFooterEvent != timelineCurrent) {
    timelineFooterEvent = timelineCurrent;
    const TimelineEvent& e = timelineData->events[timelineCurrent];
    const bool credit = e.hasImage && e.credit[0];
    modelLabel = credit ? e.credit : timelineTitle();
    modelSourceName = credit ? "" : "Wikidata";  // A credit needs the room; the line is too short for both
  }
  eduDrawFooter(c, TIMELINE_COUNTER_W);
  char counter[12];
  const bool last = timelineCurrent == timelineData->eventCount - 1;
  if (last) {
    snprintf(counter, sizeof(counter), "%s", timelineEnglish ? "End" : "Ende");
  } else {
    snprintf(counter, sizeof(counter), "%d/%d", timelineCurrent + 1, timelineData->eventCount);
  }
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::baseline_right);
  c.setTextColor(TFT_LIGHTGREY);
  c.drawString(counter, WIDTH - 2, HEIGHT - 4);
}

static void timelineLogFrame(unsigned long frameStart) {
  const uint32_t us = micros() - frameStart;
  timelineFrames++;
  timelineDrawUsSum += us;
  if (us > timelineDrawUsMax) timelineDrawUsMax = us;
  if (timelineLastFrameStart) timelinePeriodUsSum += frameStart - timelineLastFrameStart;
  timelineLastFrameStart = frameStart;
  if (millis() - timelineLogStart < TIMELINE_LOG_MS) return;
  // draw: this function, decodes included. period: start to start, showing it on the screen included
  Serial.printf("Timeline frame: draw avg %.1f max %.1f ms, period avg %.1f ms, %u decodes avg %.1f max %.1f ms, "
                "%d slots, sprite %s, heap %u\n",
                timelineDrawUsSum / 1000.0f / timelineFrames, timelineDrawUsMax / 1000.0f,
                timelinePeriodUsSum / 1000.0f / timelineFrames, (unsigned)timelineDecodes,
                timelineDecodes ? timelineDecodeUsSum / 1000.0f / timelineDecodes : 0.0f, timelineDecodeUsMax / 1000.0f,
                timelineSlotCount, modelViewSpriteInPsram() ? "psram" : "internal", (unsigned)ESP.getFreeHeap());
  timelineResetLog();
}

static void timelineFrame(M5Canvas& c, float dt) {
  const unsigned long frameStart = micros();
  const int step = timelineTiltUpdate(timelineTilt, TIMELINE_TILT_SIGN * modelGyroDps[2], dt);
  if (step != 0) timelineGo(timelineCurrent + step);  // Stops at both ends: a wrist never leaves the view
  timelinePosition = timelineMove(timelinePosition, timelineTarget, dt);
  timelineDecodeNext();

  c.fillSprite(TFT_BLACK);
  const int near = timelineNearest(*timelineData, timelinePosition);
  for (int event = max(0, near - 1); event <= min(timelineData->eventCount - 1, near + 1); event++) {
    timelineDrawCard(c, event);
  }
  timelineDrawAxis(c);
  timelineDrawYear(c);
  timelineDrawFooter(c);
  timelineLogFrame(frameStart);
}

static void timelineButtonB() {
  timelineGo(timelineCurrent - 1);
}

// Next event. Past the last one: back to the text, and the next visit starts from the beginning.
static bool timelineButtonC() {
  if (timelineCurrent < timelineData->eventCount - 1) {
    timelineGo(timelineCurrent + 1);
    return true;
  }
  timelineCurrent = 0;
  timelinePosition = timelineTarget = 0;
  return false;
}

// B: previous event, except at the first. C: next, or the written answer after the last.
static void timelineHints(int& kindB, int& kindC) {
  kindB = timelineCurrent > 0 ? HINT_BACK : HINT_NONE;
  kindC = (timelineData && timelineCurrent < timelineData->eventCount - 1) ? HINT_NEXT : HINT_TEXT;
}

static const char TIMELINE_PROMPT_RULE[] =
    "ZEITSTRAHL: Setze 'model' auf 'timeline:<id>' NUR wenn der Nutzer ausdruecklich einen Zeitstrahl, eine Zeitleiste "
    "oder die zeitliche Abfolge von Ereignissen sehen will (z.B. 'Zeig mir den Zeitstrahl der Weltkriege' ergibt "
    "'timeline:world_wars', 'Was geschah nacheinander bei den Roemern?' ergibt 'timeline:roman_empire'). "
    "Bei einzelnen Fragen zur Geschichte ('Wann war der Erste Weltkrieg?', 'Wer war Caesar?') bleibt model leer. "
    "Erlaubte <id>, nur genau diese: " TIMELINE_TOPIC_LIST ". Passt keine, bleibt model leer. "
    "Erfinde nie Daten oder Ereignisse: du nennst nur die id, die Daten kommen aus einer Datenbank. "
    "Fuelle image_query auch dann wie immer.";

const EduFeature FEATURE_TIMELINE = {
    "timeline", "Wikidata", TIMELINE_PROMPT_RULE,
    false, timelineFetch, timelineUnload, timelineBegin, timelineFrame, timelineEnd, timelineButtonB, timelineButtonC,
    nullptr, nullptr, timelineHints};
