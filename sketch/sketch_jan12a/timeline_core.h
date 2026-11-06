// Pure logic of the timeline view: file parsing, band layout, year text and the tilt gesture.
// Plain C++ with no Arduino includes, tested on the host: tests/timeline/run.sh
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "timeline_topics.h"

#define TIMELINE_MAX_EVENTS 24
#define TIMELINE_IMAGE_W 240
#define TIMELINE_IMAGE_H 96
#define TIMELINE_TEXT_LIMIT 8192             // Bytes of one <id>.txt
#define TIMELINE_BUNDLE_LIMIT (512UL * 1024) // Bytes of one <id>.jpgs
#define TIMELINE_LABEL_LIMIT 64
#define TIMELINE_CREDIT_LIMIT 80

// Band layout, in pixels. Equal time is equal distance, except what the two limits say.
static const float TIMELINE_CARD_SPACE = 240.0f;   // One picture fills the screen
static const float TIMELINE_GAP_TYPICAL = 120.0f;  // The usual distance between two events
static const float TIMELINE_GAP_MIN = 24.0f;       // Events close in time stay apart
static const float TIMELINE_GAP_MAX = 720.0f;      // A longer time is shortened, and marked as a break
static const float TIMELINE_EXACT_PX = TIMELINE_CARD_SPACE / 2;  // While its picture fills the screen, the year is the event's

// Band motion
static const float TIMELINE_SPEED_MAX = 900.0f;  // Pixels per second: a long gap takes longer to cross
static const float TIMELINE_SPEED_MIN = 60.0f;
static const float TIMELINE_EASE = 6.0f;

// Tilt gesture
static const float TIMELINE_TILT_DEADZONE_DPS = 15.0f;
static const float TIMELINE_TILT_STEP_DEG = 20.0f;
static const float TIMELINE_TILT_REST_S = 0.3f;

struct TimelineEvent {
  const char* qid;
  int32_t year;     // Negative is BC. Never 0
  int32_t endYear;  // 0: no end
  bool circa;
  bool born;  // The year is this person's birth
  const char* labelDe;
  const char* labelEn;
  const char* credit;  // Empty: nothing to show on screen
  uint32_t imageOffset, imageLength, imageHash;
  uint16_t imageW, imageH;
  bool hasImage;
  float bandX;       // Middle of this event on the band
  bool breakAfter;   // The time to the next event is longer than its distance says
};

struct Timeline {
  const char* id;
  const char* subject;
  const char* titleDe;
  const char* titleEn;
  uint32_t bundleSize;
  int eventCount;
  float pxPerYear;
  int32_t tickYears;
  TimelineEvent events[TIMELINE_MAX_EVENTS];
};

// Only ids on the closed list get as far as a URL; what the AI wrote never does.
inline bool timelineIsTopic(const char* id) {
  if (!id) return false;
  for (const char* topic : TIMELINE_TOPICS) {
    if (strcmp(topic, id) == 0) return true;
  }
  return false;
}

inline bool timelineValidText(const char* s, size_t limit, bool mayBeEmpty) {
  const size_t len = strlen(s);
  if (len > limit || (!mayBeEmpty && len == 0)) return false;
  for (size_t i = 0; i < len;) {
    const unsigned char c = (unsigned char)s[i];
    int extra;
    if (c < 0x20 || c == 0x7F) return false;
    if (c < 0x80) extra = 0;
    else if (c >= 0xC2 && c <= 0xDF) extra = 1;  // Device font ends at U+05FF: two bytes at most
    else return false;
    for (int k = 1; k <= extra; k++) {
      if (i + k >= len || ((unsigned char)s[i + k] & 0xC0) != 0x80) return false;
    }
    i += extra + 1;
  }
  return true;
}

// Strict decimal: only digits with an optional minus, nothing around them, and it has to fit.
inline bool timelineParseInt(const char* s, int64_t minimum, int64_t maximum, int64_t& out) {
  const char* p = s;
  if (*p == '-') p++;
  if (!*p || strlen(p) > 12) return false;
  for (const char* d = p; *d; d++) {
    if (*d < '0' || *d > '9') return false;
  }
  int64_t v = 0;
  for (; *p; p++) v = v * 10 + (*p - '0');
  if (*s == '-') v = -v;
  if (v < minimum || v > maximum) return false;
  out = v;
  return true;
}

inline bool timelineParseHash(const char* s, uint32_t& out) {
  if (strlen(s) != 8) return false;
  uint32_t v = 0;
  for (int i = 0; i < 8; i++) {
    const char c = s[i];
    int d;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else return false;
    v = (v << 4) | (uint32_t)d;
  }
  out = v;
  return true;
}

// Splits on tabs in place. Returns the field count; more than maxFields gives maxFields + 1.
inline int timelineSplit(char* line, char** fields, int maxFields) {
  int n = 0;
  char* p = line;
  while (true) {
    if (n == maxFields) return maxFields + 1;
    fields[n++] = p;
    char* tab = strchr(p, '\t');
    if (!tab) return n;
    *tab = 0;
    p = tab + 1;
  }
}

inline void timelineLayout(Timeline& t);

// Parses <id>.txt in place: the text must stay alive as long as the Timeline, and needs
// room for one more byte than textLen. False on anything unexpected.
inline bool timelineParse(char* text, size_t textLen, size_t capacity, const char* expectedId, Timeline& out) {
  memset(&out, 0, sizeof(out));
  if (!text || !expectedId || textLen == 0 || textLen >= capacity || textLen > TIMELINE_TEXT_LIMIT) return false;
  text[textLen] = 0;
  if (strlen(text) != textLen) return false;  // A NUL inside

  char* lineStart = text;
  char* nl = strchr(lineStart, '\n');
  if (!nl) return false;
  *nl = 0;
  char* fields[12];
  if (timelineSplit(lineStart, fields, 7) != 7) return false;
  if (strcmp(fields[0], "ZTL1") != 0 || strcmp(fields[1], expectedId) != 0) return false;
  int64_t count, bundle;
  if (!timelineParseInt(fields[5], 1, TIMELINE_MAX_EVENTS, count)) return false;
  if (!timelineParseInt(fields[6], 0, TIMELINE_BUNDLE_LIMIT, bundle)) return false;
  if (!timelineValidText(fields[2], 16, false) || !timelineValidText(fields[3], TIMELINE_LABEL_LIMIT, false) ||
      !timelineValidText(fields[4], TIMELINE_LABEL_LIMIT, false)) {
    return false;
  }
  out.id = fields[1];
  out.subject = fields[2];
  out.titleDe = fields[3];
  out.titleEn = fields[4];
  out.bundleSize = (uint32_t)bundle;

  lineStart = nl + 1;
  int32_t previousYear = INT32_MIN;
  for (int i = 0; i < (int)count; i++) {
    nl = strchr(lineStart, '\n');
    if (!nl) return false;  // Cut off: a timeline with a missing end looks finished, so refuse it
    *nl = 0;
    if (timelineSplit(lineStart, fields, 12) != 12) return false;
    TimelineEvent& e = out.events[i];
    int64_t year, endYear = 0, offset, length, w, h;
    if (fields[0][0] != 'Q' || !timelineValidText(fields[0], 16, false)) return false;
    if (!timelineParseInt(fields[1], INT32_MIN + 1, INT32_MAX, year) || year == 0) return false;
    if (strcmp(fields[2], "-") != 0) {
      if (!timelineParseInt(fields[2], INT32_MIN + 1, INT32_MAX, endYear) || endYear <= year) return false;
    }
    if (fields[3][0] < '0' || fields[3][0] > '3' || fields[3][1] != 0) return false;  // Bit 0 circa, bit 1 born
    if (!timelineValidText(fields[4], TIMELINE_LABEL_LIMIT, false)) return false;
    if (!timelineValidText(fields[5], TIMELINE_LABEL_LIMIT, false)) return false;
    if (!timelineParseInt(fields[6], 0, TIMELINE_BUNDLE_LIMIT, offset)) return false;
    if (!timelineParseInt(fields[7], 0, TIMELINE_BUNDLE_LIMIT, length)) return false;
    if (!timelineParseInt(fields[8], 0, TIMELINE_IMAGE_W, w)) return false;
    if (!timelineParseInt(fields[9], 0, TIMELINE_IMAGE_H, h)) return false;
    if (year < previousYear) return false;  // The band can only run one way
    previousYear = (int32_t)year;
    e.qid = fields[0];
    e.year = (int32_t)year;
    e.endYear = (int32_t)endYear;
    e.circa = (fields[3][0] - '0') & 1;
    e.born = (fields[3][0] - '0') & 2;
    e.labelDe = fields[4];
    e.labelEn = fields[5];
    e.credit = "";
    if (strcmp(fields[11], "-") != 0 && timelineValidText(fields[11], TIMELINE_CREDIT_LIMIT, false)) {
      e.credit = fields[11];
    }
    // A broken picture only loses the picture, never the event
    uint32_t hash = 0;
    if (length > 0 && w > 0 && h > 0 && offset + length <= bundle && timelineParseHash(fields[10], hash)) {
      e.imageOffset = (uint32_t)offset;
      e.imageLength = (uint32_t)length;
      e.imageHash = hash;
      e.imageW = (uint16_t)w;
      e.imageH = (uint16_t)h;
      e.hasImage = true;
    }
    lineStart = nl + 1;
  }
  if (*lineStart) return false;  // More lines than it says
  out.eventCount = (int)count;
  timelineLayout(out);
  return true;
}

inline uint32_t timelineFnv1a(const uint8_t* data, size_t length) {
  uint32_t h = 0x811C9DC5UL;
  for (size_t i = 0; i < length; i++) h = (h ^ data[i]) * 0x01000193UL;
  return h;
}

// Keeps a picture only if all of it arrived and it's byte for byte the one the list names.
// have: bytes received, which may be fewer than the bundle after a slow download.
// Returns how many pictures are left.
inline int timelineCheckImages(Timeline& t, const uint8_t* bundle, size_t have) {
  int kept = 0;
  for (int i = 0; i < t.eventCount; i++) {
    TimelineEvent& e = t.events[i];
    if (!e.hasImage) continue;
    const uint64_t last = (uint64_t)e.imageOffset + e.imageLength;
    bool ok = bundle && last <= have && e.imageLength >= 4;
    if (ok) {
      const uint8_t* j = bundle + e.imageOffset;
      ok = j[0] == 0xFF && j[1] == 0xD8 && j[e.imageLength - 2] == 0xFF && j[e.imageLength - 1] == 0xD9 &&
           timelineFnv1a(j, e.imageLength) == e.imageHash;
    }
    e.hasImage = ok;
    if (ok) kept++;
  }
  return kept;
}

inline int32_t timelineNiceStep(float wanted) {
  int64_t base = 1;
  while (base < 2000000000LL / 10) {
    if ((float)base >= wanted) return (int32_t)base;
    if ((float)(base * 2) >= wanted) return (int32_t)(base * 2);
    if ((float)(base * 5) >= wanted) return (int32_t)(base * 5);
    base *= 10;
  }
  return 1000000000;
}

// Where each event sits on the band. Time runs at one scale for the whole timeline, set by the
// typical distance between its events, so equal time is equal distance. Same-year events get the
// minimum gap. A time too long for that scale is shortened and marked: breakAfter.
inline void timelineLayout(Timeline& t) {
  float gaps[TIMELINE_MAX_EVENTS];
  int positive = 0;
  for (int i = 0; i + 1 < t.eventCount; i++) {
    const float dt = (float)((double)t.events[i + 1].year - (double)t.events[i].year);
    if (dt > 0) gaps[positive++] = dt;
  }
  float median = 1;
  if (positive > 0) {
    for (int i = 1; i < positive; i++) {
      const float v = gaps[i];
      int k = i - 1;
      while (k >= 0 && gaps[k] > v) {
        gaps[k + 1] = gaps[k];
        k--;
      }
      gaps[k + 1] = v;
    }
    median = gaps[positive / 2];
  }
  t.pxPerYear = TIMELINE_GAP_TYPICAL / median;
  t.tickYears = timelineNiceStep(24.0f / t.pxPerYear);
  float x = 0;
  for (int i = 0; i < t.eventCount; i++) {
    TimelineEvent& e = t.events[i];
    e.bandX = x;
    e.breakAfter = false;
    if (i + 1 < t.eventCount) {
      const float dt = (float)((double)t.events[i + 1].year - (double)e.year);
      float gap = dt * t.pxPerYear;
      if (gap > TIMELINE_GAP_MAX) {
        gap = TIMELINE_GAP_MAX;
        e.breakAfter = true;
      }
      if (gap < TIMELINE_GAP_MIN) gap = TIMELINE_GAP_MIN;
      x += TIMELINE_CARD_SPACE + gap;
    }
  }
}

// The event nearest to a band position
inline int timelineNearest(const Timeline& t, float position) {
  int best = 0;
  for (int i = 1; i < t.eventCount; i++) {
    if (fabsf(t.events[i].bandX - position) < fabsf(t.events[best].bandX - position)) best = i;
  }
  return best;
}

// One step of the band's motion towards target. Fast at first, never faster than the top speed.
inline float timelineMove(float position, float target, float dt) {
  const float distance = target - position;
  float speed = fabsf(distance) * TIMELINE_EASE;
  if (speed > TIMELINE_SPEED_MAX) speed = TIMELINE_SPEED_MAX;
  if (speed < TIMELINE_SPEED_MIN) speed = TIMELINE_SPEED_MIN;
  const float step = speed * dt;
  if (step >= fabsf(distance)) return target;
  return position + (distance > 0 ? step : -step);
}

// Distance per year in the gap after an event, the part of the band where the year runs. 0: same year.
// One value across a timeline, except where the minimum or maximum gap applies.
inline float timelineGapPxPerYear(const Timeline& t, int event) {
  const double years = (double)t.events[event + 1].year - (double)t.events[event].year;
  if (years <= 0) return 0;
  return (float)((t.events[event + 1].bandX - t.events[event].bandX - TIMELINE_CARD_SPACE) / years);
}

// The year under a band position. Sets exactEvent to the event if the position is on it, else -1.
// Across the gap between two pictures the year runs evenly from one event's year to the next;
// it's never 0, there was no year 0.
inline int32_t timelineYearAt(const Timeline& t, float position, int& exactEvent) {
  exactEvent = -1;
  if (t.eventCount == 0) return 1;
  const int near = timelineNearest(t, position);
  if (fabsf(t.events[near].bandX - position) <= TIMELINE_EXACT_PX) {
    exactEvent = near;
    return t.events[near].year;
  }
  int i = near;
  if (position < t.events[near].bandX) i = near - 1;
  if (i < 0) {
    exactEvent = 0;
    return t.events[0].year;
  }
  if (i + 1 >= t.eventCount) {
    exactEvent = t.eventCount - 1;
    return t.events[t.eventCount - 1].year;
  }
  const TimelineEvent& a = t.events[i];
  const TimelineEvent& b = t.events[i + 1];
  const double f = (position - a.bandX - TIMELINE_CARD_SPACE / 2) / (b.bandX - a.bandX - TIMELINE_CARD_SPACE);
  const double y = (double)a.year + f * ((double)b.year - (double)a.year);
  int32_t rounded = (int32_t)floor(y + 0.5);
  if (rounded == 0) rounded = y < 0 ? -1 : 1;
  return rounded;
}

// Tilt: the wrist tips, the band moves one event. Tipping back afterwards does nothing.
// (Position following the hand's angle can't work: a wrist has far less range than the band is long,
// and turning back would undo the move.)
struct TimelineTilt {
  float angle;
  float restTime;
  int8_t blockedDirection;
};

// One gyro sample in degrees per second. Returns +1 or -1 for a step, 0 otherwise.
inline int timelineTiltUpdate(TimelineTilt& s, float dps, float dt) {
  if (fabsf(dps) < TIMELINE_TILT_DEADZONE_DPS) {
    s.restTime += dt;
    if (s.restTime >= TIMELINE_TILT_REST_S) {
      s.blockedDirection = 0;
      s.angle = 0;
    }
    return 0;
  }
  s.restTime = 0;
  const int direction = dps > 0 ? 1 : -1;
  if (direction == s.blockedDirection) return 0;
  if (s.angle * direction < 0) s.angle = 0;
  s.angle += dps * dt;
  if (fabsf(s.angle) >= TIMELINE_TILT_STEP_DEG) {
    s.angle = 0;
    s.blockedDirection = (int8_t)-direction;
    return direction;
  }
  return 0;
}

// Deep time: 66, or 7,2 in German and 7.2 in English below ten million.
inline void timelineFormatMillions(int32_t year, bool english, char* out, size_t size) {
  const double millions = fabs((double)year) / 1000000.0;
  if (millions >= 10) {
    snprintf(out, size, "%ld", (long)floor(millions + 0.5));
    return;
  }
  const long tenths = (long)floor(millions * 10 + 0.5);
  snprintf(out, size, "%ld%c%ld", tenths / 10, english ? '.' : ',', tenths % 10);
}

static const int32_t TIMELINE_DEEP_TIME = -1000000;

// The year as text: big is the number, small what it means ("v. Chr.", "Mio. Jahre her", "geb.").
// Dates always come from the file; this only writes them down.
inline void timelineFormatYear(int32_t year, int32_t endYear, bool circa, bool born, bool english, char* big,
                               size_t bigSize, char* small, size_t smallSize) {
  char first[16], second[16];
  const char* prefix = circa ? (english ? "c. " : "ca. ") : "";
  small[0] = 0;
  if (year <= TIMELINE_DEEP_TIME) {
    timelineFormatMillions(year, english, first, sizeof(first));
    if (endYear != 0 && endYear <= TIMELINE_DEEP_TIME) {
      timelineFormatMillions(endYear, english, second, sizeof(second));
      snprintf(big, bigSize, "%s%s-%s", prefix, first, second);
    } else {
      snprintf(big, bigSize, "%s%s", prefix, first);
    }
    snprintf(small, smallSize, "%s", english ? "million years ago" : "Mio. Jahre her");
    return;
  }
  const char* bc = english ? "BC" : "v. Chr.";
  const char* bornText = english ? "born" : "geb.";
  if (born) {
    if (year < 0) {
      snprintf(big, bigSize, "%s%ld", prefix, -(long)year);
      snprintf(small, smallSize, "%s %s", bc, bornText);
    } else {
      snprintf(big, bigSize, "%s%ld", prefix, (long)year);
      snprintf(small, smallSize, "%s", bornText);
    }
    return;
  }
  if (year < 0) {
    if (endYear < 0) {
      snprintf(big, bigSize, "%s%ld-%ld", prefix, -(long)year, -(long)endYear);
      snprintf(small, smallSize, "%s", bc);
    } else if (endYear > 0) {
      snprintf(big, bigSize, "%s%ld %s-%ld", prefix, -(long)year, bc, (long)endYear);
    } else {
      snprintf(big, bigSize, "%s%ld", prefix, -(long)year);
      snprintf(small, smallSize, "%s", bc);
    }
    return;
  }
  if (endYear > 0) {
    snprintf(big, bigSize, "%s%ld-%ld", prefix, (long)year, (long)endYear);
  } else {
    snprintf(big, bigSize, "%s%ld", prefix, (long)year);
  }
}

// The time between two events as text: "21 Jahre", "1 Jahr", "2 Mio. Jahre"
inline void timelineFormatSpan(int32_t fromYear, int32_t toYear, bool english, char* out, size_t size) {
  // Going from BC to AD skips the year 0 that never was
  double years = (double)toYear - (double)fromYear;
  if (fromYear < 0 && toYear > 0) years -= 1;
  if (years >= 1000000.0) {
    char number[16];
    timelineFormatMillions((int32_t)-years, english, number, sizeof(number));
    snprintf(out, size, english ? "%s million years" : "%s Mio. Jahre", number);
    return;
  }
  const long whole = (long)years;
  if (english) {
    snprintf(out, size, whole == 1 ? "%ld year" : "%ld years", whole);
  } else {
    snprintf(out, size, whole == 1 ? "%ld Jahr" : "%ld Jahre", whole);
  }
}

// Cuts a label to two lines of at most maxWidth, breaking at spaces and hyphens where it can and
// never inside a UTF-8 character. width measures bytes of text; "..." marks what didn't fit.
typedef int (*TimelineMeasure)(const char* text, void* context);

inline void timelineCopyFitting(const char* text, size_t count, int maxWidth, TimelineMeasure measure, void* context,
                                char* out, size_t outSize, bool ellipsis) {
  if (count >= outSize) count = outSize - 1;
  while (count > 0 && ((unsigned char)text[count] & 0xC0) == 0x80) count--;
  while (true) {
    memcpy(out, text, count);
    out[count] = 0;
    if (ellipsis) strncat(out, "...", outSize - count - 1);
    if (count == 0 || measure(out, context) <= maxWidth) return;
    count--;
    while (count > 0 && ((unsigned char)text[count] & 0xC0) == 0x80) count--;
  }
}

inline void timelineWrapLabel(const char* text, int maxWidth, TimelineMeasure measure, void* context, char* line1,
                              char* line2, size_t lineSize) {
  line1[0] = line2[0] = 0;
  const size_t len = strlen(text);
  if (measure(text, context) <= maxWidth && len < lineSize) {
    memcpy(line1, text, len + 1);
    return;
  }
  // The longest first line that ends at a break and fits
  char trial[TIMELINE_LABEL_LIMIT + 4];
  size_t breakAt = 0;
  for (size_t i = 1; i < len && i < sizeof(trial) - 1; i++) {
    const bool atSpace = text[i] == ' ';
    const bool afterHyphen = text[i - 1] == '-';
    if (!atSpace && !afterHyphen) continue;
    memcpy(trial, text, i);
    trial[i] = 0;
    if (measure(trial, context) <= maxWidth) breakAt = i;
  }
  if (breakAt == 0) {
    // One long word: as much as fits, then the rest
    size_t count = len < lineSize ? len : lineSize - 1;
    timelineCopyFitting(text, count, maxWidth, measure, context, line1, lineSize, false);
    breakAt = strlen(line1);
  } else {
    timelineCopyFitting(text, breakAt, maxWidth, measure, context, line1, lineSize, false);
  }
  const char* rest = text + breakAt;
  while (*rest == ' ') rest++;
  const size_t restLen = strlen(rest);
  if (restLen < lineSize && measure(rest, context) <= maxWidth) {
    memcpy(line2, rest, restLen + 1);
  } else {
    timelineCopyFitting(rest, restLen, maxWidth, measure, context, line2, lineSize, true);
  }
}
