// Host tests for the world clock's pure logic. Build and run: tests/clock/run.sh
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include "../../sketch/sketch_jan12a/clock_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, e) CHECK(fabs((a) - (b)) <= (e))

static int64_t utcOf(int y, int mo, int d, int h, int mi) { return clockDaysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60; }

static void testCalendar() {
  ClockDate d;
  clockCivil(utcOf(2026, 11, 8, 7, 45), d);
  CHECK(d.year == 2026 && d.month == 11 && d.day == 8 && d.hour == 7 && d.minute == 45);
  CHECK(d.weekday == 0);  // A Sunday
  clockCivil(0, d);
  CHECK(d.year == 1970 && d.month == 1 && d.day == 1 && d.weekday == 4);
  clockCivil(-1, d);
  CHECK(d.year == 1969 && d.month == 12 && d.day == 31 && d.hour == 23 && d.second == 59);
  clockCivil(utcOf(2028, 2, 29, 12, 0), d);
  CHECK(d.month == 2 && d.day == 29 && d.weekday == 2);
  // Round trip over many years
  for (int64_t day = -20000; day < 60000; day += 37) {
    clockCivil(day * 86400, d);
    CHECK(clockDaysFromCivil(d.year, d.month, d.day) == day);
  }
}

static void testRules() {
  ClockRule r;
  CHECK(clockParseRule("CET-1CEST,M3.5.0,M10.5.0/3", r));
  CHECK(r.stdOffset == 3600 && r.dstOffset == 7200 && r.hasDst);
  // Zurich: summer time from 2026-03-29 01:00 UTC to 2026-10-25 01:00 UTC
  CHECK(clockUtcOffset(r, utcOf(2026, 3, 29, 0, 59)) == 3600);
  CHECK(clockUtcOffset(r, utcOf(2026, 3, 29, 1, 0)) == 7200);
  CHECK(clockUtcOffset(r, utcOf(2026, 10, 25, 0, 59)) == 7200);
  CHECK(clockUtcOffset(r, utcOf(2026, 10, 25, 1, 0)) == 3600);
  CHECK(clockParseRule("IST-5:30", r) && r.stdOffset == 19800 && !r.hasDst);
  CHECK(clockParseRule("<+0545>-5:45", r) && r.stdOffset == 20700);
  CHECK(clockParseRule("AEST-10AEDT,M10.1.0,M4.1.0/3", r));
  CHECK(clockUtcOffset(r, utcOf(2026, 1, 15, 0, 0)) == 39600);  // Sydney summer
  CHECK(clockUtcOffset(r, utcOf(2026, 7, 15, 0, 0)) == 36000);
  CHECK(clockParseRule("EST5EDT,0/0,J365/25", r));  // Daylight time all year, as tzdata writes it
  CHECK(clockUtcOffset(r, utcOf(2026, 6, 1, 0, 0)) == -4 * 3600);
  CHECK(clockUtcOffset(r, utcOf(2026, 12, 31, 12, 0)) == -4 * 3600);
  CHECK(!clockParseRule("", r));
  CHECK(!clockParseRule("CET", r));
  CHECK(!clockParseRule("CET-1CEST", r));  // No dates: not in our table, refused rather than guessed
  CHECK(!clockParseRule("CET-1CEST,M13.5.0,M10.5.0", r));
  CHECK(!clockParseRule("CET-1CEST,M3.5.0,M10.5.0/3x", r));
  CHECK(!clockParseRule("<+05", r));

  char out[64];
  CHECK(clockLibraryRule("<+0545>-5:45", out, sizeof(out)) && strcmp(out, "LST-5:45") == 0);
  CHECK(clockLibraryRule("<-03>3<-02>,M3.5.0/-2,M10.5.0/-1", out, sizeof(out)) &&
        strcmp(out, "LST3LDT,M3.5.0/-2,M10.5.0/-1") == 0);
  CHECK(clockLibraryRule("CET-1CEST,M3.5.0,M10.5.0/3", out, sizeof(out)) && strcmp(out, "CET-1CEST,M3.5.0,M10.5.0/3") == 0);
  CHECK(!clockLibraryRule("CET-1CEST,M3.5.0,M10.5.0/3", out, 8));
}

// Every zone in the table against this computer's tz database, every 3 hours for four years
static void testZonesAgainstSystem() {
  int compared = 0, wrong = 0;
  for (int z = 0; z < CLOCK_ZONE_COUNT; z++) {
    ClockRule r;
    CHECK(clockParseRule(CLOCK_ZONES[z].posix, r));
    char lib[64];
    CHECK(clockLibraryRule(CLOCK_ZONES[z].posix, lib, sizeof(lib)));
    std::string tz = std::string(":") + CLOCK_ZONES[z].iana;
    setenv("TZ", tz.c_str(), 1);
    tzset();
    for (int64_t t = utcOf(2026, 11, 1, 0, 0); t < utcOf(2030, 11, 1, 0, 0); t += 3 * 3600) {
      time_t tt = (time_t)t;
      struct tm local;
      localtime_r(&tt, &local);
      compared++;
      if (local.tm_gmtoff != clockUtcOffset(r, t)) {
        if (wrong++ < 5) fprintf(stderr, "  zone %s at %lld: system %ld, ours %d\n", CLOCK_ZONES[z].iana, (long long)t,
                                 (long)local.tm_gmtoff, clockUtcOffset(r, t));
      }
    }
  }
  unsetenv("TZ");
  tzset();
  checks++;
  if (wrong) failures++;
  printf("  zones: %d comparisons, %d differ\n", compared, wrong);
}

static void testNames() {
  struct { const char* name; const char* en; } cases[] = {
      {"Tokyo", "Tokyo"}, {"Tokio", "Tokyo"}, {"TOKYO", "Tokyo"}, {"Zurich", "Z\xC3\xBCrich"}, {"Z\xC3\xBCrich", "Z\xC3\xBCrich"},
      {"Zuerich", "Z\xC3\xBCrich"}, {"New York City", "New York City"}, {"new york", "New York City"}, {"Peking", "Beijing"},
      {"Beijing", "Beijing"}, {"Genf", "Geneva"}, {"Moskau", "Moscow"}, {"Sydney", "Sydney"}, {"Bern", "Bern"},
      {"Washington DC", "Washington"}, {"Munich", "Munich"}, {"M\xC3\xBCnchen", "Munich"}, {"Muenchen", "Munich"},
      {"Sao Paulo", "S\xC3\xA3o Paulo"}, {"Kathmandu", "Kathmandu"},
  };
  for (auto& c : cases) {
    const int i = clockFindCity(c.name);
    CHECK(i >= 0);
    if (i >= 0 && strcmp(CLOCK_CITIES[i].en, c.en) != 0) fprintf(stderr, "  %s found %s\n", c.name, CLOCK_CITIES[i].en);
    if (i >= 0) CHECK(strcmp(CLOCK_CITIES[i].en, c.en) == 0);
  }
  CHECK(clockFindCity("Atlantis") < 0);
  CHECK(clockFindCity("") < 0);
  CHECK(clockFindCity("!!!") < 0);
  CHECK(clockFindCity("\xFF\xFE") < 0);
  CHECK(clockFindCity("Mars") < 0);
  // Every city is found by its own names, or the name belongs to a capital or a bigger city of that name
  for (int i = 0; i < CLOCK_CITY_COUNT; i++) {
    for (const char* n : {CLOCK_CITIES[i].en, CLOCK_CITIES[i].de}) {
      const int j = clockFindCity(n);
      CHECK(j >= 0);
      if (j != i && j >= 0) {
        char a[64], b[64], c[64];
        clockFold(n, a, sizeof a);
        clockFold(CLOCK_CITIES[j].en, b, sizeof b);
        clockFold(CLOCK_CITIES[j].de, c, sizeof c);
        CHECK(strcmp(a, b) == 0 || strcmp(a, c) == 0 || j < i || CLOCK_CITIES[j].capital);
      }
    }
    CHECK(CLOCK_CITIES[i].zone < CLOCK_ZONE_COUNT);
    CHECK(abs(CLOCK_CITIES[i].lat100) <= 9000 && abs(CLOCK_CITIES[i].lon100) <= 18000);
  }
  // Keys sorted, so the binary search works
  for (int k = 1; k < CLOCK_KEY_COUNT; k++) CHECK(strcmp(CLOCK_KEYS[k - 1].key, CLOCK_KEYS[k].key) < 0);
  CHECK(strcmp(CLOCK_CITIES[CLOCK_HOME_DEFAULT].en, "Z\xC3\xBCrich") == 0);
  // Names must fit the stick's fonts: nothing beyond U+017F
  for (int i = 0; i < CLOCK_CITY_COUNT; i++) {
    for (const char* n : {CLOCK_CITIES[i].en, CLOCK_CITIES[i].de}) {
      for (const unsigned char* p = (const unsigned char*)n; *p; p++) CHECK(*p < 0xC6 || (*p & 0xC0) == 0x80);
    }
  }
}

static void testSun() {
  float lat, lon;
  // March equinox 2026: 2026-03-20 14:46 UTC (USNO). The Sun is over the equator.
  clockSubsolarPoint((double)utcOf(2026, 3, 20, 14, 46), lat, lon);
  NEAR(lat, 0.0, 0.02);
  // June solstice 2026-06-21 08:24 UTC: over the Tropic of Cancer
  clockSubsolarPoint((double)utcOf(2026, 6, 21, 8, 24), lat, lon);
  NEAR(lat, 23.43, 0.02);
  // December solstice 2026-12-21 20:50 UTC
  clockSubsolarPoint((double)utcOf(2026, 12, 21, 20, 50), lat, lon);
  NEAR(lat, -23.43, 0.02);
  // Around 3 November the equation of time is about +16.4 min: noon at Greenwich comes at 11:43:36 UTC
  clockSubsolarPoint((double)utcOf(2026, 11, 3, 11, 43) + 36, lat, lon);
  NEAR(lon, 0.0, 0.15);
  // Around 11 February it is about -14.2 min: noon at 12:14:12 UTC
  clockSubsolarPoint((double)utcOf(2026, 2, 11, 12, 14) + 12, lat, lon);
  NEAR(lon, 0.0, 0.15);
  // The Sun moves 15 degrees west per hour
  float lon2;
  clockSubsolarPoint((double)utcOf(2026, 2, 11, 13, 14) + 12, lat, lon2);
  NEAR(lon2, -15.0, 0.2);
}

static void testAnswer() {
  char out[256];
  const int tokyo = clockFindCity("Tokyo"), zurich = clockFindCity("Zurich"), delhi = clockFindCity("New Delhi");
  const int64_t t = utcOf(2026, 11, 8, 6, 32);  // 07:32 in Zurich (winter time), 15:32 in Tokyo
  clockAnswer(out, sizeof out, true, tokyo, zurich, t);
  CHECK(strcmp(out, "In Tokio ist es jetzt 15:32 Uhr, Sonntag. Das ist 8 Stunden sp\xC3\xA4ter als in Z\xC3\xBCrich.") == 0);
  clockAnswer(out, sizeof out, false, tokyo, zurich, t);
  CHECK(strcmp(out, "In Tokyo it is 15:32 now, Sunday. That is 8 hours later than in Z\xC3\xBCrich.") == 0);
  CHECK(delhi >= 0);
  clockAnswer(out, sizeof out, true, delhi, zurich, t);
  CHECK(strstr(out, "12:02 Uhr") && strstr(out, "4 Stunden 30 Minuten sp\xC3\xA4ter"));
  clockAnswer(out, sizeof out, true, zurich, tokyo, t);
  CHECK(strstr(out, "8 Stunden fr\xC3\xBC" "her als in Tokio"));
  clockAnswer(out, sizeof out, true, zurich, zurich, t);
  CHECK(strcmp(out, "Es ist jetzt 07:32 Uhr, Sonntag, in Z\xC3\xBCrich.") == 0);
  clockAnswer(out, sizeof out, true, clockFindCity("Bern"), zurich, t);
  CHECK(strstr(out, "dieselbe Zeit"));
  clockFormatGap(out, sizeof out, 3600, true);
  CHECK(strcmp(out, "1 Stunde") == 0);
  clockFormatGap(out, sizeof out, 45 * 60, false);
  CHECK(strcmp(out, "45 minutes") == 0);
  clockFormatOffset(out, sizeof out, 19800);
  CHECK(strcmp(out, "UTC+5:30") == 0);
  clockFormatOffset(out, sizeof out, -3 * 3600);
  CHECK(strcmp(out, "UTC-3") == 0);
  clockNoAnswer(out, sizeof out, true, true, "Atlantis");
  CHECK(strstr(out, "Atlantis"));
  // A hostile query is only ever printed, and cut to fit
  std::string longQuery(500, 'x');
  clockNoAnswer(out, sizeof out, false, true, longQuery.c_str());
  CHECK(strlen(out) < sizeof out);
}

int main() {
  testCalendar();
  testRules();
  testZonesAgainstSystem();
  testNames();
  testSun();
  testAnswer();
  printf("clock: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
