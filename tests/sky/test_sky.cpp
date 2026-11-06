// Host tests for the sky view's data. Build and run: tests/sky/run.sh
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../../sketch/sketch_jan12a/sky_data.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int findLatin(const char* latin) {
  for (int i = 0; i < SKY_CONSTELLATION_COUNT; i++) {
    if (strcmp(SKY_CONSTELLATIONS[i].latin, latin) == 0) return i;
  }
  return -1;
}

static bool validUtf8InFont(const char* s) {
  const unsigned char* p = (const unsigned char*)s;
  while (*p) {
    if (*p < 0x20 || *p == 0x7F) return false;
    if (*p < 0x80) { p++; continue; }
    if ((*p & 0xE0) != 0xC0 || (p[1] & 0xC0) != 0x80) return false;  // U+0080-U+07FF only
    const int code = ((*p & 0x1F) << 6) | (p[1] & 0x3F);
    if (code > 0x5FF) return false;
    p += 2;
  }
  return true;
}

static void testStars() {
  CHECK(SKY_STAR_COUNT >= 1630);  // BSC stars down to magnitude 5
  CHECK(SKY_STAR_COUNT < 1800);
  CHECK((int)(sizeof(SKY_STARS) / sizeof(SKY_STARS[0])) == SKY_STAR_COUNT);
  int dimmer = 0;
  for (int i = 0; i < SKY_STAR_COUNT; i++) {
    const SkyStar& s = SKY_STARS[i];
    const double length = sqrt((double)s.x * s.x + (double)s.y * s.y + (double)s.z * s.z);
    CHECK(fabs(length - 32767) < 3);
    CHECK(s.band <= 5);
    if (i > 0) CHECK(s.mag10 >= SKY_STARS[i - 1].mag10);  // Brightest first
    if (s.mag10 > (SKY_MAG_LIMIT + 2) * 10) dimmer++;
  }
  CHECK(dimmer > 0 && dimmer <= 35);  // Only the 35 dim stars lines end on; 5.04 still rounds to the limit
  CHECK(SKY_STARS[0].mag10 == 5);  // Sirius, magnitude -1.46
}

static void testConstellations() {
  CHECK(SKY_CONSTELLATION_COUNT == 88);  // The IAU's 88, Serpens counted once
  int total = 0;
  for (int i = 0; i < SKY_CONSTELLATION_COUNT; i++) {
    const SkyConstellation& c = SKY_CONSTELLATIONS[i];
    CHECK(c.lineStart == total);
    CHECK(c.lineCount > 0);
    total += c.lineCount;
    CHECK(c.radiusDeg > 0 && c.radiusDeg < 90);
    CHECK(validUtf8InFont(c.latin));
    CHECK(validUtf8InFont(c.german));
    CHECK(c.mainCount <= SKY_MAIN_STARS);
    for (int k = 0; k < c.mainCount; k++) {
      CHECK(c.mainStar[k] < SKY_STAR_COUNT);
      CHECK(c.mainName[k] && c.mainName[k][0]);
      if (k > 0) CHECK(SKY_STARS[c.mainStar[k]].mag10 >= SKY_STARS[c.mainStar[k - 1]].mag10);
    }
    // Every line star lies inside the circle given for the constellation
    const double cosLimit = cos((c.radiusDeg + 0.5) * M_PI / 180);
    for (int k = c.lineStart * 2; k < (c.lineStart + c.lineCount) * 2; k++) {
      CHECK(SKY_LINES[k] < SKY_STAR_COUNT);
      const SkyStar& s = SKY_STARS[SKY_LINES[k]];
      const double d = ((double)s.x * c.x + (double)s.y * c.y + (double)s.z * c.z) / (32767.0 * 32767.0);
      CHECK(d >= cosLimit);
    }
  }
  CHECK(total == SKY_LINE_COUNT);
  CHECK((int)(sizeof(SKY_LINES) / sizeof(SKY_LINES[0])) == SKY_LINE_COUNT * 2);
}

static void testKnownEntries() {
  const int orion = findLatin("Orion");
  CHECK(orion >= 0);
  if (orion >= 0) {
    const SkyConstellation& c = SKY_CONSTELLATIONS[orion];
    CHECK(c.mainCount == 3);
    CHECK(strcmp(c.mainName[0], "Rigel") == 0);
    CHECK(strcmp(c.mainName[1], "Betelgeuse") == 0);
    CHECK(strcmp(c.mainName[2], "Bellatrix") == 0);
  }
  CHECK(findLatin("Serpens") >= 0);
  CHECK(findLatin("Serpens Caput") < 0);
  const int bootes = findLatin("Bo\303\266tes");
  CHECK(bootes >= 0);
  const int bear = findLatin("Ursa Major");
  CHECK(bear >= 0);
  if (bear >= 0) CHECK(strcmp(SKY_CONSTELLATIONS[bear].german, "Gro\303\237er B\303\244r") == 0);
}

static void testKeys() {
  CHECK(SKY_KEY_COUNT >= 88);
  for (int i = 0; i < SKY_KEY_COUNT; i++) {
    CHECK(SKY_KEYS[i].constellation < SKY_CONSTELLATION_COUNT);
    CHECK(strcmp(SKY_KEYS[i].key, "all") != 0);
    if (i > 0) CHECK(strcmp(SKY_KEYS[i - 1].key, SKY_KEYS[i].key) < 0);  // Sorted, no duplicates
    for (const char* p = SKY_KEYS[i].key; *p; p++) CHECK(*p >= 'a' && *p <= 'z');
  }
}

int main() {
  testStars();
  testConstellations();
  testKnownEntries();
  testKeys();
  printf("sky tests: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
