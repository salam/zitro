// Pure logic of the world clock: finding a city by name, time zone rules with daylight saving time,
// calendar dates, where the Sun stands overhead, and the written answer. Plain C++ with no Arduino
// includes, tested on the host: tests/clock/run.sh
//
// The rules are the IANA tz database's POSIX TZ strings (clock_data.h), evaluated here rather than by the
// C library: the view shows many zones at once, and switching the library's one global zone per city
// would be slow and would disturb the device clock that timers and the parental lock use.
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "clock_data.h"

static const double CLOCK_PI = 3.14159265358979323846;

// ---------- Names ----------

// Lower-case a-z only. Accented Latin letters lose their marks (CLOCK_FOLD_LATIN, made by the same
// script that made the keys), everything else is dropped: "Zürich", "zurich" and "Zu-rich" are the same.
inline void clockFold(const char* in, char* out, size_t size) {
  size_t n = 0;
  const unsigned char* s = (const unsigned char*)in;
  while (*s && n + 1 < size) {
    const unsigned char c = *s;
    if (c >= 'a' && c <= 'z') {
      out[n++] = (char)c;
      s++;
    } else if (c >= 'A' && c <= 'Z') {
      out[n++] = (char)(c - 'A' + 'a');
      s++;
    } else if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
      const unsigned cp = ((c & 0x1Fu) << 6) | (s[1] & 0x3Fu);
      if (cp >= 0xC0 && cp < 0x180) {
        for (const char* f = CLOCK_FOLD_LATIN[cp - 0xC0]; *f && n + 1 < size; f++) out[n++] = *f;
      }
      s += 2;
    } else if (c >= 0x80) {
      // Longer UTF-8 sequences, or broken bytes: skip the whole character
      s++;
      while ((*s & 0xC0) == 0x80) s++;
    } else {
      s++;
    }
  }
  out[n] = 0;
}

// The city a name means, or -1. Only names in the table count: the AI's word is never trusted further.
inline int clockFindCity(const char* query) {
  char key[64];
  clockFold(query, key, sizeof(key));
  if (!key[0]) return -1;
  int low = 0, high = CLOCK_KEY_COUNT - 1;
  while (low <= high) {
    const int mid = (low + high) / 2;
    const int cmp = strcmp(CLOCK_KEYS[mid].key, key);
    if (cmp == 0) return CLOCK_KEYS[mid].city;
    if (cmp < 0) low = mid + 1;
    else high = mid - 1;
  }
  return -1;
}

inline const char* clockCityName(int city, bool german) {
  return german ? CLOCK_CITIES[city].de : CLOCK_CITIES[city].en;
}

// ---------- Calendar ----------

// Days since 1970-01-01 for a date, and back (Howard Hinnant's algorithms; any year)
inline int64_t clockDaysFromCivil(int64_t y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

struct ClockDate {
  int64_t year;
  int month, day, hour, minute, second;
  int weekday;  // 0 Sunday
};

inline int64_t clockFloorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

inline void clockCivil(int64_t seconds, ClockDate& out) {
  const int64_t days = clockFloorDiv(seconds, 86400);
  int64_t rest = seconds - days * 86400;
  out.hour = (int)(rest / 3600);
  rest %= 3600;
  out.minute = (int)(rest / 60);
  out.second = (int)(rest % 60);
  out.weekday = (int)(((days % 7) + 11) % 7);  // 1970-01-01 was a Thursday
  const int64_t z = days + 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  out.day = (int)(doy - (153 * mp + 2) / 5 + 1);
  out.month = (int)(mp < 10 ? mp + 3 : mp - 9);
  out.year = yoe + era * 400 + (out.month <= 2);
}

inline bool clockLeapYear(int64_t y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

// ---------- POSIX TZ rules ----------

struct ClockTransition {
  char kind;     // 'M' month.week.day, 'J' day 1-365 without Feb 29, 'D' day 0-365 with it
  int month, week, weekday, day;
  int32_t time;  // Seconds after local midnight; may be negative or past 24 h (RFC 8536)
};

struct ClockRule {
  int32_t stdOffset;  // Seconds east of UTC (POSIX writes west, this is turned round)
  int32_t dstOffset;
  bool hasDst;
  ClockTransition start, end;
};

inline bool clockParseName(const char*& p) {
  if (*p == '<') {
    const char* q = strchr(p, '>');
    if (!q || q - p < 4) return false;
    p = q + 1;
    return true;
  }
  int n = 0;
  while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
    p++;
    n++;
  }
  return n >= 3;
}

inline bool clockParseNumber(const char*& p, int& out) {
  if (*p < '0' || *p > '9') return false;
  out = 0;
  while (*p >= '0' && *p <= '9') {
    out = out * 10 + (*p - '0');
    if (out > 100000) return false;
    p++;
  }
  return true;
}

// [+|-]hh[:mm[:ss]] in seconds
inline bool clockParseTime(const char*& p, int32_t& out) {
  int sign = 1;
  if (*p == '+') p++;
  else if (*p == '-') {
    sign = -1;
    p++;
  }
  int h = 0, m = 0, s = 0;
  if (!clockParseNumber(p, h) || h > 167) return false;
  if (*p == ':') {
    p++;
    if (!clockParseNumber(p, m) || m > 59) return false;
    if (*p == ':') {
      p++;
      if (!clockParseNumber(p, s) || s > 59) return false;
    }
  }
  out = sign * (h * 3600 + m * 60 + s);
  return true;
}

inline bool clockParseTransition(const char*& p, ClockTransition& t) {
  t.time = 2 * 3600;
  t.month = t.week = t.weekday = t.day = 0;
  if (*p == 'M') {
    p++;
    t.kind = 'M';
    if (!clockParseNumber(p, t.month) || t.month < 1 || t.month > 12 || *p++ != '.') return false;
    if (!clockParseNumber(p, t.week) || t.week < 1 || t.week > 5 || *p++ != '.') return false;
    if (!clockParseNumber(p, t.weekday) || t.weekday > 6) return false;
  } else if (*p == 'J') {
    p++;
    t.kind = 'J';
    if (!clockParseNumber(p, t.day) || t.day < 1 || t.day > 365) return false;
  } else {
    t.kind = 'D';
    if (!clockParseNumber(p, t.day) || t.day > 365) return false;
  }
  if (*p == '/') {
    p++;
    if (!clockParseTime(p, t.time)) return false;
  }
  return true;
}

inline bool clockParseRule(const char* text, ClockRule& r) {
  const char* p = text;
  memset(&r, 0, sizeof(r));
  int32_t west;
  if (!clockParseName(p) || !clockParseTime(p, west)) return false;
  r.stdOffset = -west;
  if (!*p) return true;
  if (!clockParseName(p)) return false;
  r.hasDst = true;
  r.dstOffset = r.stdOffset + 3600;
  if (*p && *p != ',') {
    if (!clockParseTime(p, west)) return false;
    r.dstOffset = -west;
  }
  // Without a rule, POSIX leaves the dates to the implementation. Every zone in the table has one.
  if (*p++ != ',' || !clockParseTransition(p, r.start) || *p++ != ',' || !clockParseTransition(p, r.end)) return false;
  return *p == 0;
}

// Local midnight of the transition's day, as days since 1970
inline int64_t clockTransitionDay(const ClockTransition& t, int64_t year) {
  if (t.kind == 'J') {
    int64_t day = clockDaysFromCivil(year, 1, 1) + t.day - 1;
    if (clockLeapYear(year) && t.day >= 60) day++;  // Feb 29 is never counted
    return day;
  }
  if (t.kind == 'D') return clockDaysFromCivil(year, 1, 1) + t.day;
  const int64_t first = clockDaysFromCivil(year, t.month, 1);
  const int firstWeekday = (int)(((first % 7) + 11) % 7);
  int64_t day = first + ((t.weekday - firstWeekday + 7) % 7) + 7 * (t.week - 1);
  static const int monthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int length = monthDays[t.month - 1] + (t.month == 2 && clockLeapYear(year));
  while (day >= first + length) day -= 7;  // Week 5 means the last one
  return day;
}

inline bool clockIsDst(const ClockRule& r, int64_t utc) {
  if (!r.hasDst) return false;
  ClockDate d;
  clockCivil(utc + r.stdOffset, d);
  const int64_t start = clockTransitionDay(r.start, d.year) * 86400 + r.start.time - r.stdOffset;
  const int64_t end = clockTransitionDay(r.end, d.year) * 86400 + r.end.time - r.dstOffset;
  if (start < end) return utc >= start && utc < end;
  return utc < end || utc >= start;  // Southern hemisphere: daylight time runs over New Year
}

inline int32_t clockUtcOffset(const ClockRule& r, int64_t utc) { return clockIsDst(r, utc) ? r.dstOffset : r.stdOffset; }

inline bool clockZoneRule(int zone, ClockRule& r) {
  return zone >= 0 && zone < CLOCK_ZONE_COUNT && clockParseRule(CLOCK_ZONES[zone].posix, r);
}

// The device clock's TZ for configTzTime. newlib may not read names in <...>, which only name the zone and
// don't change any time: they become plain letters.
inline bool clockLibraryRule(const char* posix, char* out, size_t size) {
  size_t n = 0;
  int names = 0;
  for (const char* p = posix; *p; p++) {
    if (*p == '<') {
      const char* q = strchr(p, '>');
      if (!q) return false;
      const char* name = names++ == 0 ? "LST" : "LDT";
      for (const char* s = name; *s; s++) {
        if (n + 1 >= size) return false;
        out[n++] = *s;
      }
      p = q;
    } else {
      if (n + 1 >= size) return false;
      out[n++] = *p;
    }
  }
  out[n] = 0;
  return true;
}

// ---------- The Sun ----------

// Where the Sun stands overhead: latitude is its declination, longitude follows Greenwich's time and the
// equation of time. Astronomical Almanac low-precision formulas, about 0.01 degrees for 1950-2050.
inline void clockSubsolarPoint(double unixSeconds, float& latDeg, float& lonDeg) {
  const double n = unixSeconds / 86400.0 + 2440587.5 - 2451545.0;
  const double rad = CLOCK_PI / 180.0;
  double L = fmod(280.460 + 0.9856474 * n, 360.0);
  double g = fmod(357.528 + 0.9856003 * n, 360.0);
  if (L < 0) L += 360.0;
  if (g < 0) g += 360.0;
  const double lambda = (L + 1.915 * sin(g * rad) + 0.020 * sin(2 * g * rad)) * rad;
  const double eps = (23.439 - 0.0000004 * n) * rad;
  const double alpha = atan2(cos(eps) * sin(lambda), cos(lambda));  // Right ascension
  const double delta = asin(sin(eps) * sin(lambda));
  // Greenwich mean sidereal time in degrees; the Sun is overhead where the local sidereal time is alpha
  double gmst = fmod(280.46061837 + 360.98564736629 * n, 360.0);
  double lon = alpha / rad - gmst;
  lon = fmod(lon + 540.0, 360.0) - 180.0;
  latDeg = (float)(delta / rad);
  lonDeg = (float)lon;
}

// ---------- The answer ----------

static const char* const CLOCK_WEEKDAY_DE[7] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag",
                                                "Samstag"};
static const char* const CLOCK_WEEKDAY_EN[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
                                                "Saturday"};

// "8 Stunden", "1 Stunde 30 Minuten", "45 minutes"
inline void clockFormatGap(char* out, size_t size, int32_t seconds, bool german) {
  const int minutes = seconds / 60;
  const int h = minutes / 60, m = minutes % 60;
  char hours[32] = "", mins[32] = "";
  if (h) snprintf(hours, sizeof(hours), "%d %s", h, german ? (h == 1 ? "Stunde" : "Stunden") : (h == 1 ? "hour" : "hours"));
  if (m) snprintf(mins, sizeof(mins), "%d %s", m, german ? (m == 1 ? "Minute" : "Minuten") : (m == 1 ? "minute" : "minutes"));
  snprintf(out, size, "%s%s%s", hours, (h && m) ? " " : "", mins);
}

// The written answer. Everything in it is computed: the AI's own words about times never reach the screen.
inline void clockAnswer(char* out, size_t size, bool german, int city, int home, int64_t utc) {
  ClockRule cityRule, homeRule;
  if (!clockZoneRule(CLOCK_CITIES[city].zone, cityRule) || !clockZoneRule(CLOCK_CITIES[home].zone, homeRule)) {
    snprintf(out, size, "%s", german ? "Die Zeitzone kenne ich nicht." : "I don't know that time zone.");
    return;
  }
  const int32_t cityOffset = clockUtcOffset(cityRule, utc), homeOffset = clockUtcOffset(homeRule, utc);
  ClockDate d;
  clockCivil(utc + cityOffset, d);
  const char* name = clockCityName(city, german);
  const char* homeName = clockCityName(home, german);
  const char* weekday = german ? CLOCK_WEEKDAY_DE[d.weekday] : CLOCK_WEEKDAY_EN[d.weekday];
  char first[128];
  if (city == home) {
    snprintf(first, sizeof(first), german ? "Es ist jetzt %02d:%02d Uhr, %s, in %s." : "It is %02d:%02d now, %s, in %s.",
             d.hour, d.minute, weekday, name);
    snprintf(out, size, "%s", first);
    return;
  }
  snprintf(first, sizeof(first), german ? "In %s ist es jetzt %02d:%02d Uhr, %s." : "In %s it is %02d:%02d now, %s.", name,
           d.hour, d.minute, weekday);
  const int32_t gap = cityOffset - homeOffset;
  char second[128];
  if (gap == 0) {
    snprintf(second, sizeof(second), german ? "Das ist dieselbe Zeit wie in %s." : "That is the same time as in %s.",
             homeName);
  } else {
    char amount[64];
    clockFormatGap(amount, sizeof(amount), gap < 0 ? -gap : gap, german);
    if (german) {
      snprintf(second, sizeof(second), "Das ist %s %s als in %s.", amount, gap > 0 ? "sp\xC3\xA4ter" : "fr\xC3\xBCher",
               homeName);
    } else {
      snprintf(second, sizeof(second), "That is %s %s than in %s.", amount, gap > 0 ? "later" : "earlier", homeName);
    }
  }
  snprintf(out, size, "%s %s", first, second);
}

// The honest answer when there is no clock to show
inline void clockNoAnswer(char* out, size_t size, bool german, bool clockKnown, const char* query) {
  if (!clockKnown) {
    snprintf(out, size, "%s",
             german ? "Ich kenne die Uhrzeit gerade nicht. Ich brauche dazu WLAN." : "I don't know the time right now. I need WiFi for it.");
  } else {
    snprintf(out, size, german ? "Die Uhrzeit von %s kenne ich nicht. Frag mich nach einer Hauptstadt oder grossen Stadt."
                               : "I don't know the time in %s. Ask me about a capital or a big city.",
             query);
  }
}

// "14:32" in a city now
inline bool clockCityTime(int city, int64_t utc, ClockDate& out, int32_t& offset) {
  ClockRule r;
  if (!clockZoneRule(CLOCK_CITIES[city].zone, r)) return false;
  offset = clockUtcOffset(r, utc);
  clockCivil(utc + offset, out);
  return true;
}

// "UTC+5:30", "UTC-3", "UTC"
inline void clockFormatOffset(char* out, size_t size, int32_t offset) {
  if (offset == 0) {
    snprintf(out, size, "UTC");
    return;
  }
  const int32_t a = offset < 0 ? -offset : offset;
  if (a % 3600) snprintf(out, size, "UTC%c%d:%02d", offset < 0 ? '-' : '+', (int)(a / 3600), (int)(a % 3600 / 60));
  else snprintf(out, size, "UTC%c%d", offset < 0 ? '-' : '+', (int)(a / 3600));
}
