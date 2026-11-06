// Host tests for the sky view's data. Build and run: tests/sky/run.sh
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../../sketch/sketch_jan12a/sky_core.h"

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

static int nameIndex(int constellation, const char* name) {
  const SkyConstellation& c = SKY_CONSTELLATIONS[constellation];
  for (int k = 0; k < c.mainCount; k++) {
    if (strcmp(c.mainName[k], name) == 0) return c.mainStar[k];
  }
  return -1;
}

static void view(const Mat3& r, int star, float p[3]) {
  const SkyStar& s = SKY_STARS[star];
  rotatePoint(r, s.x / 32767.0f, s.y / 32767.0f, s.z / 32767.0f, p);
}

static void testLookup() {
  // What the AI is likely to send
  const char* orion[] = {"Orion", "orion", "ORION", " Orion ", "the Orion", "Orion constellation", "Hunter", "Orion's Belt"};
  for (const char* q : orion) CHECK(skyFindConstellation(q) == findLatin("Orion"));
  const char* bear[] = {"Ursa Major", "ursa major", "URSA MAJOR", "ursa-major", "Great Bear", "The Great Bear", "Big Dipper",
                        "Plough", "Plow", "Gro\303\237er B\303\244r", "Grosser Baer", "Grosser Bar", "Grosser Wagen"};
  for (const char* q : bear) CHECK(skyFindConstellation(q) == findLatin("Ursa Major"));
  CHECK(skyFindConstellation("Little Dipper") == findLatin("Ursa Minor"));
  CHECK(skyFindConstellation("Southern Cross") == findLatin("Crux"));
  CHECK(skyFindConstellation("Crux") == findLatin("Crux"));
  CHECK(skyFindConstellation("Scorpio") == findLatin("Scorpius"));
  CHECK(skyFindConstellation("Scorpion") == findLatin("Scorpius"));
  CHECK(skyFindConstellation("Swan") == findLatin("Cygnus"));
  CHECK(skyFindConstellation("Bootes") == findLatin("Bo\303\266tes"));
  CHECK(skyFindConstellation("Bo\303\266tes") == findLatin("Bo\303\266tes"));
  CHECK(skyFindConstellation("Herdsman") == findLatin("Bo\303\266tes"));
  CHECK(skyFindConstellation("Serpens") == findLatin("Serpens"));
  CHECK(skyFindConstellation("Serpens Cauda") == findLatin("Serpens"));
  CHECK(skyFindConstellation("Drafting Compass") == findLatin("Circinus"));
  CHECK(skyFindConstellation("Mariner's Compass") == findLatin("Pyxis"));
  for (int i = 0; i < SKY_CONSTELLATION_COUNT; i++) {
    CHECK(skyFindConstellation(SKY_CONSTELLATIONS[i].latin) == i);
    CHECK(skyFindConstellation(SKY_CONSTELLATIONS[i].german) == i);
  }
  CHECK(skyFindConstellation("all") == SKY_WHOLE_DOME);
  CHECK(skyFindConstellation("ALL") == SKY_WHOLE_DOME);
  CHECK(skyFindConstellation("the all") == SKY_WHOLE_DOME);

  // Off the list, ambiguous, broken or hostile: never a match, never a crash
  const char* no[] = {"", " ", "...", "the", "constellation", "the constellation", "Compass", "Mars", "Sirius", "Andromeda Galaxy",
                      "Orion; DROP TABLE", "Orion\n", "Orion\t", "<script>", "../../etc/passwd", "%00", "Orion1", "1", "al", "alll",
                      "\xff\xfe", "\xc3", "Or\xc3", "\xc3\x28", "\xf0\x9f\x8c\x9f", "O\xc3\xa9rion", "\xce\xa9", "%s%s%s%n"};
  for (const char* q : no) CHECK(skyFindConstellation(q) == SKY_NOT_FOUND);
  CHECK(skyFindConstellation(nullptr) == SKY_NOT_FOUND);
  std::string big(100000, 'a');
  CHECK(skyFindConstellation(big.c_str()) == SKY_NOT_FOUND);
  std::string exact(SKY_FOLD_MAX - 1, 'a'), over(SKY_FOLD_MAX, 'a');
  CHECK(skyFindConstellation(exact.c_str()) == SKY_NOT_FOUND);
  CHECK(skyFindConstellation(over.c_str()) == SKY_NOT_FOUND);
  std::string sharps;
  for (size_t i = 0; i < SKY_FOLD_MAX; i++) sharps += "\303\237";  // Doubles in length when folded
  CHECK(skyFindConstellation(sharps.c_str()) == SKY_NOT_FOUND);
  char tiny[2];
  CHECK(!skyFoldName("Orion", tiny, sizeof(tiny)));
}

static void testNotMirrored() {
  const int orion = findLatin("Orion");
  const SkyConstellation& c = SKY_CONSTELLATIONS[orion];
  Mat3 r;
  skyLookAt(c.x, c.y, c.z, r);
  float rigel[3], betelgeuse[3], bellatrix[3];
  view(r, nameIndex(orion, "Rigel"), rigel);
  view(r, nameIndex(orion, "Betelgeuse"), betelgeuse);
  view(r, nameIndex(orion, "Bellatrix"), bellatrix);
  // As in the real sky with north up: Betelgeuse east of Bellatrix (to the left), Rigel below both, Rigel right of Betelgeuse
  CHECK(betelgeuse[0] < bellatrix[0]);
  CHECK(betelgeuse[0] < rigel[0]);
  CHECK(rigel[1] < betelgeuse[1]);
  CHECK(rigel[1] < bellatrix[1]);
  CHECK(betelgeuse[2] < 0 && rigel[2] < 0 && bellatrix[2] < 0);

  // Ursa Major: Alkaid at the handle's end lies east of Dubhe, so on the left
  const int bear = findLatin("Ursa Major");
  skyLookAt(SKY_CONSTELLATIONS[bear].x, SKY_CONSTELLATIONS[bear].y, SKY_CONSTELLATIONS[bear].z, r);
  float alkaid[3], dubhe[3];
  view(r, nameIndex(bear, "Alkaid"), alkaid);
  view(r, nameIndex(bear, "Dubhe"), dubhe);
  CHECK(dubhe[2] < 0 && alkaid[2] < 0);
  CHECK(alkaid[0] < dubhe[0]);
  const float det = r.m[0] * (r.m[4] * r.m[8] - r.m[5] * r.m[7]) - r.m[1] * (r.m[3] * r.m[8] - r.m[5] * r.m[6]) +
                    r.m[2] * (r.m[3] * r.m[7] - r.m[4] * r.m[6]);
  CHECK(fabs(det - 1) < 1e-4);  // A proper turn, never a reflection

  // Sirius is east of Orion and south: left and below when looking at Orion
  Mat3 o;
  skyLookAt(c.x, c.y, c.z, o);
  float sirius[3];
  view(o, 0, sirius);
  CHECK(sirius[0] < betelgeuse[0]);
  CHECK(sirius[1] < betelgeuse[1]);
}

static void testLookAt() {
  for (int i = 0; i < SKY_CONSTELLATION_COUNT; i++) {
    const SkyConstellation& c = SKY_CONSTELLATIONS[i];
    Mat3 r;
    skyLookAt(c.x, c.y, c.z, r);
    float p[3];
    const float l = sqrtf((float)c.x * c.x + (float)c.y * c.y + (float)c.z * c.z);
    rotatePoint(r, c.x / l, c.y / l, c.z / l, p);
    CHECK(fabs(p[0]) < 1e-3 && fabs(p[1]) < 1e-3 && fabs(p[2] + 1) < 1e-3);
  }
  // Straight at a pole, and nonsense
  Mat3 r;
  skyLookAt(0, 0, 1, r);
  float p[3];
  rotatePoint(r, 0, 0, 1, p);
  CHECK(fabs(p[2] + 1) < 1e-5);
  skyLookAt(0, 0, -1, r);
  rotatePoint(r, 0, 0, -1, p);
  CHECK(fabs(p[2] + 1) < 1e-5);
  skyLookAt(0, 0, 0, r);
  CHECK(r.m[0] == 1 && r.m[4] == 1 && r.m[8] == 1);
  // The cheap depth test gives the same answer as the whole turn
  skyLookAt(0.3f, -0.5f, 0.7f, r);
  for (int i = 0; i < SKY_STAR_COUNT; i += 37) {
    view(r, i, p);
    CHECK(fabs(skyDepth(r, SKY_STARS[i]) / 32767.0f - p[2]) < 1e-3);
  }
}

static void testProjection() {
  float sx, sy;
  const float centre[3] = {0, 0, -1};
  CHECK(skyProject(centre, 120, 120, 58, sx, sy));
  CHECK(sx == 120 && sy == 58);
  const float rightUp[3] = {0.5f, 0.25f, -1};
  CHECK(skyProject(rightUp, 120, 120, 58, sx, sy));
  CHECK(sx == 180 && sy == 28);  // Right is right, up is smaller y
  const float behind[3] = {0, 0, 1};
  CHECK(!skyProject(behind, 120, 120, 58, sx, sy));
  const float side[3] = {1, 0, 0};
  CHECK(!skyProject(side, 120, 120, 58, sx, sy));

  float a[3] = {0, 0, -1}, b[3] = {1, 0, 1};
  CHECK(skyClipLine(a, b));
  CHECK(fabs(b[2] + SKY_NEAR) < 1e-6 && b[0] > 0 && b[0] < 1);
  CHECK(skyProject(b, 480, 120, 58, sx, sy));
  CHECK(fabs(sx) < 1e5 && fabs(sy) < 1e5);  // Stays well inside what the line drawing takes
  float c[3] = {1, 0, 1}, d[3] = {0, 0, -1};
  CHECK(skyClipLine(c, d));
  CHECK(fabs(c[2] + SKY_NEAR) < 1e-6);
  float e[3] = {1, 0, 1}, f[3] = {0, 1, 1};
  CHECK(!skyClipLine(e, f));
  float g[3] = {0, 0, -1}, h[3] = {0, 1, -1};
  CHECK(skyClipLine(g, h));
  CHECK(g[2] == -1 && h[2] == -1);
}

static void testZoomAndArrow() {
  // Crux is small and needs zooming in, Hydra never fits and starts wide
  CHECK(skyFitZoom(SKY_CONSTELLATIONS[findLatin("Crux")].radiusDeg, 50) == 2);
  CHECK(skyFitZoom(SKY_CONSTELLATIONS[findLatin("Hydra")].radiusDeg, 50) == 0);
  CHECK(skyFitZoom(SKY_CONSTELLATIONS[findLatin("Orion")].radiusDeg, 50) == 0);
  CHECK(skyFitZoom(255, 50) == 0);
  float x, y, ux, uy;
  skyEdgePoint(10, 0, 120, 58, 100, 40, x, y, ux, uy);
  CHECK(fabs(x - 220) < 1e-3 && fabs(y - 58) < 1e-3 && fabs(ux - 1) < 1e-5);
  skyEdgePoint(0, -10, 120, 58, 100, 40, x, y, ux, uy);
  CHECK(fabs(x - 120) < 1e-3 && fabs(y - 18) < 1e-3 && fabs(uy + 1) < 1e-5);
  skyEdgePoint(0, 0, 120, 58, 100, 40, x, y, ux, uy);  // Exactly behind
  CHECK(x >= 20 && x <= 220 && y >= 18 && y <= 98);
  skyEdgePoint(1e8f, -1e8f, 120, 58, 100, 40, x, y, ux, uy);
  CHECK(x >= 19.9f && x <= 220.1f && y >= 17.9f && y <= 98.1f);
}

static void testAppearance() {
  CHECK(skyStarClass(0) == 0);
  CHECK(skyStarClass(5) == 0);   // Sirius
  CHECK(skyStarClass(70) == 5);  // Magnitude 5
  CHECK(skyStarClass(255) == 5);
  CHECK(skyStarRadius(0) > skyStarRadius(1));
  CHECK(skyStarRadius(5) == 0);
  // Brighter is never darker, in every colour band
  for (uint8_t band = 0; band <= 5; band++) {
    for (uint8_t cls = 1; cls <= 5; cls++) {
      CHECK(skyStarColor565(cls, band) != 0);
      CHECK((skyStarColor565(cls - 1, band) >> 11) >= (skyStarColor565(cls, band) >> 11));
    }
  }
  CHECK(skyStarColor565(0, 2) == 0xFFFF);  // Neutral band is white
  // A hint of colour: blue stars have more blue than red, orange ones more red than blue
  CHECK((skyStarColor565(0, 0) & 0x1F) > (skyStarColor565(0, 0) >> 11));
  CHECK((skyStarColor565(0, 5) >> 11) > (skyStarColor565(0, 5) & 0x1F));
  CHECK(skyStarColor565(200, 200) == skyStarColor565(5, 2));
  // Real stars: Betelgeuse orange-red, Rigel blue
  const int orion = findLatin("Orion");
  CHECK(SKY_STARS[nameIndex(orion, "Betelgeuse")].band == 5);
  CHECK(SKY_STARS[nameIndex(orion, "Rigel")].band == 0 || SKY_STARS[nameIndex(orion, "Rigel")].band == 1);
}

int main() {
  testStars();
  testConstellations();
  testKnownEntries();
  testKeys();
  testLookup();
  testNotMirrored();
  testLookAt();
  testProjection();
  testZoomAndArrow();
  testAppearance();
  printf("sky tests: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
