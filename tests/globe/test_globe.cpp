// Host tests for the globe's pure logic. Build and run: tests/globe/run.sh
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>
#include "../../sketch/sketch_jan12a/globe_coast_data.h"
#include "../../sketch/sketch_jan12a/globe_core.h"
#include "../../sketch/sketch_jan12a/globe_wiki.h"
#include "../../sketch/sketch_jan12a/model_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, e) CHECK(fabs((a) - (b)) <= (e))

static std::string fixtureDir, detailPath;
static std::string readFile(const std::string& path) {
  std::string out;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) { fprintf(stderr, "missing file %s\n", path.c_str()); exit(2); }
  char buf[4096]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
  fclose(f);
  return out;
}
static bool wiki(const std::string& s, GlobePlace& p) { return globeParseWikiReply(s.c_str(), s.size(), p); }
static bool wikiFile(const char* name, GlobePlace& p) { return wiki(readFile(fixtureDir + "/" + name), p); }

static void testRealReplies() {
  GlobePlace p;
  CHECK(wikiFile("Matterhorn.json", p));
  CHECK(strcmp(p.title, "Matterhorn") == 0);
  CHECK(strcmp(p.titleDe, "Matterhorn") == 0);
  NEAR(p.lat, 45.976, 0.001);
  NEAR(p.lon, 7.659, 0.001);
  CHECK(wikiFile("Eiffel_Tower.json", p));
  CHECK(strcmp(p.titleDe, "Eiffelturm") == 0);
  CHECK(wikiFile("North_Pole.json", p));
  NEAR(p.lat, 90, 0.001);
  CHECK(p.titleDe[0] == 0);
  CHECK(wikiFile("Pacific_Ocean.json", p));
  NEAR(p.lon, -160, 0.001);
  // No coordinates, not on Earth, or no article: never a globe
  const char* none[] = {"Mars.json", "Photosynthesis.json", "Atlantis.json", "Missing.json", "Olympus_Mons.json",
                        "Tycho.json", "Sea_of_Tranquility.json"};
  for (const char* name : none) {
    CHECK(!wikiFile(name, p));
    CHECK(p.lat == 0 && p.title[0] == 0);
  }
}

static std::string page(const std::string& inner) {
  return "{\"query\":{\"pages\":[{\"pageid\":1,\"ns\":0,\"title\":\"X\"," + inner + "}]}}";
}
static std::string coord(const std::string& lat, const std::string& lon, const std::string& globe) {
  return page("\"coordinates\":[{\"lat\":" + lat + ",\"lon\":" + lon + ",\"primary\":true" + globe + "}]");
}

static void testHostileReplies() {
  GlobePlace p;
  const std::string earth = ",\"globe\":\"earth\"";
  CHECK(wiki(coord("1", "2", earth), p));
  CHECK(wiki(coord("-90", "-180", earth), p));
  CHECK(wiki(coord("90", "180", earth), p));
  const std::string bad[] = {
      "", "{", "[]", "null", "{\"query\":{}}", "{\"query\":{\"pages\":[]}}", "{\"query\":{\"pages\":{}}}",
      "{\"query\":{\"pages\":[1]}}", "{\"query\":{\"pages\":[{}]}}",
      coord("1", "2", ""),                           // No globe field
      coord("1", "2", ",\"globe\":\"Earth\""),       // Exactly "earth" only
      coord("1", "2", ",\"globe\":\"mars\""),
      coord("1", "2", ",\"globe\":\"\""),
      coord("1", "2", ",\"globe\":1"),
      coord("1", "2", ",\"globe\":null"),
      coord("\"45\"", "2", earth),                   // Numbers as strings
      coord("1", "\"7\"", earth),
      coord("null", "2", earth),
      coord("true", "2", earth),
      coord("90.01", "2", earth),
      coord("-91", "2", earth),
      coord("1", "180.5", earth),
      coord("1", "226.2", earth),
      coord("1e999", "2", earth),
      coord("NaN", "2", earth),
      page("\"coordinates\":[]"),
      page("\"coordinates\":{}"),
      page("\"coordinates\":\"here\""),
      page("\"coordinates\":[1]"),
      page("\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"},{\"lat\":3,\"lon\":4,\"globe\":\"moon\"}]"),
      // Two articles at once, as "Matterhorn|Mars" asks for
      "{\"query\":{\"pages\":[{\"ns\":0,\"title\":\"A\",\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}]},"
      "{\"ns\":0,\"title\":\"B\",\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}]}]}}",
      // Not an article
      "{\"query\":{\"pages\":[{\"ns\":1,\"title\":\"Talk:A\",\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}]}]}}",
      "{\"query\":{\"pages\":[{\"title\":\"A\",\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}]}]}}",
      "{\"query\":{\"pages\":[{\"ns\":0,\"title\":\"A\",\"missing\":true,\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}]}]}}",
      "{\"query\":{\"pages\":[{\"ns\":0,\"title\":\"A\",\"invalid\":true,\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}]}]}}",
      // Titles
      "{\"query\":{\"pages\":[{\"ns\":0,\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}]}]}}",
      "{\"query\":{\"pages\":[{\"ns\":0,\"title\":\"\",\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}]}]}}",
      "{\"query\":{\"pages\":[{\"ns\":0,\"title\":7,\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}]}]}}",
      "{\"query\":{\"pages\":[{\"ns\":0,\"title\":\"a\\nb\",\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}]}]}}",
  };
  for (const std::string& s : bad) {
    if (wiki(s, p)) fprintf(stderr, "  accepted: %s\n", s.c_str());
    CHECK(!wiki(s, p));
  }
  // Cut off anywhere: false, never a crash
  const std::string good = readFile(fixtureDir + "/Eiffel_Tower.json");
  for (size_t n = 0; n < good.size(); n++) CHECK(!globeParseWikiReply(good.c_str(), n, p));
  CHECK(!globeParseWikiReply(nullptr, 5, p));
  // Nesting far past any real reply
  CHECK(!wiki(std::string(5000, '[') + std::string(5000, ']'), p));
  // A huge title is cut to fit, inside the buffer and not inside a UTF-8 character
  std::string longTitle;
  for (int i = 0; i < 400; i++) longTitle += "\xC3\xBC";
  const std::string longReply = "{\"query\":{\"pages\":[{\"ns\":0,\"title\":\"a" + longTitle +
                                "\",\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}],\"langlinks\":[{\"lang\":\"de\",\"title\":\"" +
                                longTitle + "\"}]}]}}";
  CHECK(wiki(longReply, p));
  CHECK(strlen(p.title) <= GLOBE_TITLE_BYTES);
  CHECK(strlen(p.title) % 2 == 1);  // "a", then whole two-byte characters
  CHECK(strlen(p.titleDe) % 2 == 0);
  // A broken German name is dropped, the place stays
  const std::string oddDe = "{\"query\":{\"pages\":[{\"ns\":0,\"title\":\"A\",\"coordinates\":[{\"lat\":1,\"lon\":2,\"globe\":\"earth\"}],"
                            "\"langlinks\":[{\"lang\":\"de\",\"title\":7}]}]}}";
  CHECK(wiki(oddDe, p));
  CHECK(p.titleDe[0] == 0);
}

static std::vector<uint8_t> makeFile(const std::vector<uint16_t>& lengths, int points, int lat = 100, int lon = 200) {
  std::vector<uint8_t> f = {'Z', 'G', 'L', 'B', 1, 0};
  auto u16 = [&](int v) { f.push_back((uint8_t)v); f.push_back((uint8_t)(v >> 8)); };
  u16((int)lengths.size());
  u16(points);
  for (uint16_t n : lengths) u16(n);
  for (int i = 0; i < points; i++) { u16(lat); u16(lon); }
  return f;
}
static bool checkFile(const std::vector<uint8_t>& f) {
  uint16_t lines, points;
  const bool ok = globeCheckFile(f.data(), f.size(), lines, points);
  if (!ok) CHECK(lines == 0 && points == 0);
  return ok;
}

static void testFileFormat() {
  uint16_t lines, points;
  std::vector<uint8_t> good = makeFile({2, 3}, 5);
  CHECK(globeCheckFile(good.data(), good.size(), lines, points));
  CHECK(lines == 2 && points == 5);
  CHECK(checkFile(makeFile({2}, 2, 9000, 18000)));
  CHECK(checkFile(makeFile({2}, 2, -9000, -18000)));
  // Every cut-off length fails
  for (size_t n = 0; n < good.size(); n++) CHECK(!globeCheckFile(good.data(), n, lines, points));
  CHECK(!globeCheckFile(nullptr, 100, lines, points));
  // One byte extra fails
  std::vector<uint8_t> f = good;
  f.push_back(0);
  CHECK(!checkFile(f));
  // Wrong magic, version, reserved byte
  for (int i = 0; i < 6; i++) { f = good; f[i] ^= 0x40; CHECK(!checkFile(f)); }
  CHECK(!checkFile(makeFile({1, 4}, 5)));    // A line of one point
  CHECK(!checkFile(makeFile({0, 5}, 5)));
  CHECK(!checkFile(makeFile({2, 2}, 5)));    // Lengths don't add up
  CHECK(!checkFile(makeFile({3, 3}, 5)));
  CHECK(!checkFile(makeFile({65535, 3}, 2))); // A sum that would overflow 16 bits
  CHECK(!checkFile(makeFile({}, 0)));
  CHECK(!checkFile(makeFile({2}, 2, 9001, 0)));
  CHECK(!checkFile(makeFile({2}, 2, -9001, 0)));
  CHECK(!checkFile(makeFile({2}, 2, 0, 18001)));
  CHECK(!checkFile(makeFile({2}, 2, -32768, -32768)));
  // Sizes claimed in the header can't reach past the buffer
  f = good;
  f[8] = 0xFF; f[9] = 0xFF;
  CHECK(!checkFile(f));
  f = good;
  f[6] = 0xFF; f[7] = 0xFF;
  CHECK(!checkFile(f));
  CHECK(!checkFile(makeFile({(uint16_t)(GLOBE_MAX_POINTS + 1)}, GLOBE_MAX_POINTS + 1)));
  // Random bytes and random flips never crash; a flip in a coordinate may still pass, in range
  srand(7);
  for (int round = 0; round < 3000; round++) {
    f = good;
    for (int k = 0; k < 3; k++) f[rand() % f.size()] = (uint8_t)rand();
    if (globeCheckFile(f.data(), f.size(), lines, points)) {
      std::vector<uint16_t> l(lines);
      std::vector<int16_t> v(points * 3u);
      globeDecode(f.data(), lines, points, l.data(), v.data());
      for (int16_t c : v) CHECK(abs(c) <= 16384);
    }
  }
}

struct Map {
  uint16_t lines = 0, points = 0;
  std::vector<uint16_t> lengths;
  std::vector<int16_t> xyz;
  bool load(const uint8_t* d, size_t n) {
    if (!globeCheckFile(d, n, lines, points)) return false;
    lengths.resize(lines);
    xyz.resize(points * 3u);
    globeDecode(d, lines, points, lengths.data(), xyz.data());
    return true;
  }
};

static void testRealFiles(Map& builtin, Map& detail) {
  CHECK(builtin.load(GLOBE_COAST_BUILTIN, GLOBE_COAST_BUILTIN_SIZE));
  CHECK(builtin.points > 5000 && builtin.points <= 5128);
  const std::string d = readFile(detailPath);
  CHECK(detail.load((const uint8_t*)d.data(), d.size()));
  CHECK(detail.points > 2 * builtin.points);
  CHECK(d.size() <= GLOBE_FILE_LIMIT);
  // Every point is on the unit sphere, to rounding
  for (size_t i = 0; i + 2 < builtin.xyz.size(); i += 3) {
    const float x = builtin.xyz[i] / GLOBE_Q14, y = builtin.xyz[i + 1] / GLOBE_Q14, z = builtin.xyz[i + 2] / GLOBE_Q14;
    NEAR(x * x + y * y + z * z, 1.0f, 0.001f);
  }
  // Decoding gives the right place: the first point in the file
  const float lat = globeReadI16(GLOBE_COAST_BUILTIN + GLOBE_FILE_HEADER + 2 * builtin.lines) * 0.01f;
  const float lon = globeReadI16(GLOBE_COAST_BUILTIN + GLOBE_FILE_HEADER + 2 * builtin.lines + 2) * 0.01f;
  float v[3];
  globeUnitVector(lat, lon, v);
  NEAR(v[0], builtin.xyz[0] / GLOBE_Q14, 0.001f);
  NEAR(v[1], builtin.xyz[1] / GLOBE_Q14, 0.001f);
  NEAR(v[2], builtin.xyz[2] / GLOBE_Q14, 0.001f);
}

static void testFacing() {
  const float places[][2] = {{45.976f, 7.659f}, {0, 0}, {90, 0}, {-90, 0}, {90, 123}, {0, 180}, {0, -180}, {-33.9f, 151.2f}, {64.1f, -21.9f}};
  for (auto& place : places) {
    float m[9], pin[3], out[3];
    globeFacing(place[0], place[1], m);
    for (float f : m) CHECK(std::isfinite(f));
    globeUnitVector(place[0], place[1], pin);
    Mat3 r;
    memcpy(r.m, m, sizeof(m));
    rotatePoint(r, pin[0], pin[1], pin[2], out);
    NEAR(out[0], 0, 1e-5);   // In the middle of the screen
    NEAR(out[1], 0, 1e-5);
    NEAR(out[2], 1, 1e-5);   // On the near side
    rotatePoint(r, 0, 0, 1, out);
    CHECK(out[0] > -1e-5f && out[0] < 1e-5f);  // North is straight up
    CHECK(out[1] > -1e-5f);
    // A real rotation: determinant 1, so nothing is mirrored
    const float det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
    NEAR(det, 1, 1e-4);
  }
  // Facing the Matterhorn, Italy is south and east: lower and to the right
  float m[9], milan[3];
  globeFacing(45.976f, 7.659f, m);
  globeUnitVector(45.46f, 9.19f, milan);
  Mat3 r;
  memcpy(r.m, m, sizeof(m));
  float out[3];
  rotatePoint(r, milan[0], milan[1], milan[2], out);
  CHECK(out[0] > 0 && out[1] < 0 && out[2] > 0);
  // Q14 survives anything
  const float broken[9] = {NAN, INFINITY, -INFINITY, 5, -5, 0, 1, -1, 0.5f};
  int32_t q[9];
  globeRotationToQ14(broken, q);
  for (int32_t v : q) CHECK(v >= -16384 && v <= 16384);
}

static const int W = 240, H = 117;

static void testRaster() {
  std::vector<uint16_t> buf(W * H + 2, 0);
  uint16_t* b = buf.data() + 1;
  CHECK(globeRasterLine(b, W, H, 5, 5, 5, 5, 7) == 1);
  CHECK(b[5 * W + 5] == 7);
  CHECK(globeRasterLine(b, W, H, 10, 10, 14, 10, 7) == 5);
  CHECK(globeRasterLine(b, W, H, 10, 10, 13, 13, 7) == 4);
  CHECK(globeRasterLine(b, W, H, 10, 10, 13, 12, 7) == 4);
  CHECK(globeRasterLine(b, W, H, 13, 12, 10, 10, 7) == 4);
  // Off the screen on one side: nothing
  CHECK(globeRasterLine(b, W, H, -50, 3, -1, 80, 7) == 0);
  CHECK(globeRasterLine(b, W, H, W, 3, W + 40, 80, 7) == 0);
  CHECK(globeRasterLine(b, W, H, 3, -9, 80, -1, 7) == 0);
  CHECK(globeRasterLine(b, W, H, 3, H, 80, H + 9, 7) == 0);
  // Across the screen and past every corner: only inside pixels, and the guards stay untouched
  std::fill(buf.begin(), buf.end(), 0);
  srand(11);
  for (int i = 0; i < 20000; i++) {
    globeRasterLine(b, W, H, rand() % 1400 - 700, rand() % 1400 - 700, rand() % 1400 - 700, rand() % 1400 - 700, 7);
  }
  CHECK(buf[0] == 0);
  CHECK(buf[W * H + 1] == 0);
  CHECK(globeRasterLine(b, W, H, 0, 0, W - 1, H - 1, 9) > 200);
}

static void testSegmentsStayOnTheNearSide() {
  // A ring around the equator of the file format, seen from above the north pole: all of it near side,
  // seen from the equator: half of it, cut at the disc's edge
  const int n = 72;
  std::vector<int16_t> xyz;
  for (int i = 0; i <= n; i++) {
    float v[3];
    globeUnitVector(0, i * 5.0f - 180.0f, v);
    xyz.push_back((int16_t)lroundf(v[0] * GLOBE_Q14));
    xyz.push_back((int16_t)lroundf(v[1] * GLOBE_Q14));
    xyz.push_back((int16_t)lroundf(v[2] * GLOBE_Q14));
  }
  const uint16_t lengths[1] = {(uint16_t)(n + 1)};
  float m[9];
  int32_t q[9];
  const int cx = 120, cy = 58, radius = 55;

  globeFacing(90, 0, m);
  globeRotationToQ14(m, q);
  int count = 0;
  float maxDistance = 0, minDistance = 1e9f;
  uint32_t reported = globeForEachSegment(lengths, 1, xyz.data(), q, cx, cy, radius, [&](int x0, int y0, int x1, int y1) {
    (void)x1; (void)y1;
    const float d = hypotf((float)(x0 - cx), (float)(y0 - cy));
    maxDistance = fmaxf(maxDistance, d);
    minDistance = fminf(minDistance, d);
    count++;
  });
  CHECK((int)reported == count);
  CHECK(count == n);
  NEAR(maxDistance, radius, 2);
  NEAR(minDistance, radius, 2);

  // From the equator: the ring is the flat line through the middle, and it reaches the edge on both sides
  globeFacing(0, 0, m);
  globeRotationToQ14(m, q);
  int lowX = 9999, highX = -9999, badY = 0;
  count = 0;
  globeForEachSegment(lengths, 1, xyz.data(), q, cx, cy, radius, [&](int x0, int y0, int x1, int y1) {
    lowX = fmin(lowX, fmin(x0, x1));
    highX = fmax(highX, fmax(x0, x1));
    if (abs(y0 - cy) > 1 || abs(y1 - cy) > 1) badY++;
    count++;
  });
  CHECK(badY == 0);
  NEAR(lowX, cx - radius, 1);
  NEAR(highX, cx + radius, 1);
  CHECK(count < n);  // Near the edge, pieces collapse onto one pixel and are left out

  // A line entirely on the far side draws nothing
  globeFacing(0, 180, m);
  globeRotationToQ14(m, q);
  std::vector<int16_t> nearZero;
  for (int i = 0; i < 5; i++) {
    float v[3];
    globeUnitVector(0, i * 5.0f - 10.0f, v);
    nearZero.push_back((int16_t)lroundf(v[0] * GLOBE_Q14));
    nearZero.push_back((int16_t)lroundf(v[1] * GLOBE_Q14));
    nearZero.push_back((int16_t)lroundf(v[2] * GLOBE_Q14));
  }
  const uint16_t five[1] = {5};
  CHECK(globeForEachSegment(five, 1, nearZero.data(), q, cx, cy, radius, [&](int, int, int, int) {}) == 0);
}

static void testPin() {
  const int cx = 120, cy = 58, radius = 55;
  float m[9], pin[3];
  GlobePinView v;
  globeFacing(45.976f, 7.659f, m);
  globeUnitVector(45.976f, 7.659f, pin);
  globeLocatePin(m, pin, cx, cy, radius, W, H, v);
  CHECK(v.onScreen);
  CHECK(v.x == cx && v.y == cy);

  // Seen from the other side of the Earth: arrow, not pin
  globeFacing(-45.976f, 7.659f - 180.0f, m);
  globeLocatePin(m, pin, cx, cy, radius, W, H, v);
  CHECK(!v.onScreen);
  for (int i = 0; i < 6; i++) CHECK(!std::isnan((float)v.arrow[i]));

  // Pin behind, up and to the right of the far side: the arrow points that way
  Mat3 r;
  mat3Identity(r);
  const float behind[3] = {0.5f, 0.5f, -0.7071f};
  memcpy(m, r.m, sizeof(m));
  globeLocatePin(m, behind, cx, cy, radius, W, H, v);
  CHECK(!v.onScreen);
  const int tailX = (v.arrow[2] + v.arrow[4]) / 2, tailY = (v.arrow[3] + v.arrow[5]) / 2;
  CHECK(v.arrow[0] > tailX && v.arrow[1] < tailY);  // Right and up (screen y grows downwards)

  // Every direction, every zoom: the arrow is whole and on the screen. Exactly behind too.
  for (int zoomRadius : {55, 88, 132}) {
    for (int a = 0; a < 360; a += 3) {
      for (float back : {-1.0f, -0.5f, -0.01f, 0.0f}) {
        const float s = sqrtf(1 - back * back);
        const float p[3] = {s * cosf(a * GLOBE_DEG), s * sinf(a * GLOBE_DEG), back};
        globeLocatePin(m, p, cx, cy, zoomRadius, W, H, v);
        if (back < 0) CHECK(!v.onScreen);
        if (!v.onScreen) {
          for (int i = 0; i < 6; i += 2) {
            CHECK(v.arrow[i] >= 0 && v.arrow[i] < W);
            CHECK(v.arrow[i + 1] >= 0 && v.arrow[i + 1] < H);
          }
        }
      }
    }
  }

  // Zoomed in: on the near side but past the screen's edge is also an arrow, and it points there
  const float farRight[3] = {0.95f, 0.0f, 0.31f};
  globeLocatePin(m, farRight, cx, cy, 132, W, H, v);
  CHECK(!v.onScreen);
  CHECK(v.arrow[0] > cx);
  globeLocatePin(m, farRight, cx, cy, 55, W, H, v);
  CHECK(v.onScreen);
  // The pin's head needs room above its tip
  const float top[3] = {0.0f, 0.98f, 0.2f};
  globeLocatePin(m, top, cx, cy, 58, W, H, v);
  CHECK(!v.onScreen);
  CHECK(v.arrow[1] < cy);
}

static void step(GlobeSpin& s, float dps, float dt, float turn[3], int frames = 1) {
  const float g[3] = {0, dps, 0};
  for (int i = 0; i < frames; i++) globeSpinStep(s, g, dt, turn);
}

static void testSpin() {
  GlobeSpin s;
  float turn[3];
  const float dt = 0.04f;
  globeSpinReset(s);

  // Lying still: nothing added, so the shared view is in charge
  step(s, 1, dt, turn, 100);
  CHECK(turn[0] == 0 && turn[1] == 0 && turn[2] == 0);

  // An ordinary slow turn stops with the hand
  step(s, 80, dt, turn, 25);
  CHECK(turn[1] == 0);  // The shared view turns it while the hand moves; nothing doubled
  step(s, 0, dt, turn);
  CHECK(turn[1] == 0);

  // A flick keeps going in the direction the shared view turned it, and slows down
  globeSpinReset(s);
  step(s, 300, dt, turn, 3);
  CHECK(turn[1] == 0);
  step(s, 0, dt, turn);
  CHECK(turn[1] < 0);  // Same sign as MODEL_TURN_SIGN times the gyro
  float previous = fabsf(turn[1]);
  const float first = previous;
  for (int i = 0; i < 30; i++) {
    step(s, 0, dt, turn);
    CHECK(fabsf(turn[1]) < previous);
    previous = fabsf(turn[1]);
  }
  CHECK(previous < first * 0.5f);
  // ...and stops by itself in a few seconds
  step(s, 0, dt, turn, 200);
  CHECK(turn[1] == 0);

  // The hand's small tremor doesn't stop it, a real move catches it
  globeSpinReset(s);
  step(s, 300, dt, turn, 3);
  step(s, 10, dt, turn);
  CHECK(turn[1] < 0);
  step(s, 60, dt, turn);
  CHECK(turn[1] == 0);
  step(s, 0, dt, turn);
  CHECK(turn[1] == 0);  // Caught, and the catch itself was too slow to count as a new flick

  // A fast turn that slowed gently before stopping isn't a flick
  globeSpinReset(s);
  step(s, 300, dt, turn, 3);
  step(s, 100, dt, turn, 10);
  step(s, 0, dt, turn);
  CHECK(turn[1] == 0);

  // Very hard flick: capped, so it doesn't strobe
  globeSpinReset(s);
  step(s, 2000, dt, turn, 2);
  step(s, 0, dt, turn);
  CHECK(fabsf(turn[1]) <= GLOBE_SPIN_MAX * dt * 1.001f);
  CHECK(fabsf(turn[1]) > GLOBE_SPIN_MAX * dt * 0.9f);

  // The largest motion counts, in all three axes
  globeSpinReset(s);
  const float tilted[3] = {200, -100, 50};
  globeSpinStep(s, tilted, dt, turn);
  const float none[3] = {0, 0, 0};
  globeSpinStep(s, none, dt, turn);
  CHECK(turn[0] < 0 && turn[1] > 0 && turn[2] < 0);

  // Broken readings leave it stopped, not spinning on NaN
  globeSpinReset(s);
  const float broken[3] = {NAN, 0, 0};
  globeSpinStep(s, broken, dt, turn);
  CHECK(turn[0] == 0);
  step(s, 0, dt, turn);
  CHECK(turn[1] == 0 && !std::isnan(s.velocity[0]));
  step(s, 0, 0, turn);
  CHECK(turn[1] == 0);
}

// How much the coast costs to draw, as counted, not timed: there's no stick here. Worst view of many.
static void reportCost(const char* name, const Map& map) {
  const int cx = 120, cy = 58;
  std::vector<uint16_t> buf(W * H);
  for (int radius : {55, 88, 132}) {
    uint32_t worstSegments = 0, worstPixels = 0;
    for (int lat = -90; lat <= 90; lat += 15) {
      for (int lon = -180; lon < 180; lon += 15) {
        float m[9];
        int32_t q[9];
        globeFacing((float)lat, (float)lon, m);
        globeRotationToQ14(m, q);
        uint32_t pixels = 0;
        const uint32_t segments = globeForEachSegment(
            map.lengths.data(), map.lines, map.xyz.data(), q, cx, cy, radius,
            [&](int x0, int y0, int x1, int y1) { pixels += globeRasterLine(buf.data(), W, H, x0, y0, x1, y1, 1); });
        if (segments > worstSegments) worstSegments = segments;
        if (pixels > worstPixels) worstPixels = pixels;
      }
    }
    printf("  %s radius %3d: %5u points transformed, at most %5u pieces and %5u pixels drawn\n", name, radius,
           map.points, worstSegments, worstPixels);
    CHECK(worstSegments <= map.points);
  }
}

// Day and night: levels, and the disc filled where it should be and nowhere else
static void testShade() {
  CHECK(globeLightLevel(-1.0f) == 0);
  CHECK(globeLightLevel(-0.11f) == 0);
  CHECK(globeLightLevel(0.06f) == GLOBE_LIGHT_STEPS);
  CHECK(globeLightLevel(1.0f) == GLOBE_LIGHT_STEPS);
  CHECK(globeLightLevel(NAN) == 0);
  int last = 0;
  for (float d = -0.2f; d <= 0.2f; d += 0.001f) {
    const int l = globeLightLevel(d);
    CHECK(l >= last && l <= GLOBE_LIGHT_STEPS);
    last = l;
  }
  uint16_t palette[GLOBE_LIGHT_STEPS + 1];
  for (int i = 0; i <= GLOBE_LIGHT_STEPS; i++) palette[i] = (uint16_t)(100 + i);
  static uint16_t buf[117][240];
  auto clear = [&]() { for (auto& row : buf) for (auto& p : row) p = 1; };
  auto count = [&](uint16_t v) { int n = 0; for (auto& row : buf) for (auto p : row) n += p == v; return n; };
  // Sun behind the viewer: all day. No sun: all day too.
  const float front[3] = {0, 0, 1};
  clear();
  const uint32_t n = globeShadeDisc(&buf[0][0], 240, 117, 120, 58, 50, front, palette);
  CHECK(count(100 + GLOBE_LIGHT_STEPS) > (int)n * 9 / 10);  // The rim sees the Sun on its horizon: twilight
  CHECK(count(100) == 0);
  CHECK(count(1) == 240 * 117 - (int)n);
  NEAR((double)n, M_PI * 50 * 50, 300);
  clear();
  CHECK(globeShadeDisc(&buf[0][0], 240, 117, 120, 58, 50, nullptr, palette) == n);
  // Sun to the right: the left half is night, the right half day
  const float right[3] = {1, 0, 0};
  clear();
  globeShadeDisc(&buf[0][0], 240, 117, 120, 58, 50, right, palette);
  CHECK(buf[58][75] == 100);
  CHECK(buf[58][165] == 100 + GLOBE_LIGHT_STEPS);
  CHECK(buf[58][119] != 100 && buf[58][119] != 100 + GLOBE_LIGHT_STEPS);  // Twilight on the line
  CHECK(buf[58][60] == 1);                                                // Outside the disc untouched
  // Zoomed far past the screen: only the screen is written
  clear();
  const uint32_t big = globeShadeDisc(&buf[0][0], 240, 117, 120, 58, 400, front, palette);
  CHECK(big == 240u * 117u);
  CHECK(count(1) == 0);
  // Off the screen entirely
  clear();
  CHECK(globeShadeDisc(&buf[0][0], 240, 117, -500, -500, 50, front, palette) == 0);
  CHECK(globeShadeDisc(&buf[0][0], 240, 117, 120, 58, 0, front, palette) == 0);
  // The Sun's direction on screen follows the turn
  float m[9], s[3];
  globeFacing(0, 0, m);
  float earth[3];
  globeUnitVector(0, 0, earth);
  globeToScreen(m, earth, s);
  NEAR(s[2], 1.0, 1e-5);
  globeUnitVector(0, 90, earth);  // 90 degrees east is to the right
  globeToScreen(m, earth, s);
  NEAR(s[0], 1.0, 1e-5);
}

int main(int argc, char** argv) {
  if (argc != 3) { fprintf(stderr, "usage: test_globe fixtures detail.bin\n"); return 2; }
  fixtureDir = argv[1];
  detailPath = argv[2];
  static Map builtin, detail;
  testRealReplies();
  testHostileReplies();
  testFileFormat();
  testRealFiles(builtin, detail);
  testFacing();
  testRaster();
  testSegmentsStayOnTheNearSide();
  testPin();
  testSpin();
  testShade();
  reportCost("built-in", builtin);
  reportCost("server  ", detail);
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
