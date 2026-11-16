// Host tests for the 3D view's pure logic. Build and run: tests/model/run.sh
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../../sketch/sketch_jan12a/model_core.h"
#include "../../sketch/sketch_jan12a/button_rules.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, e) CHECK(fabs((a) - (b)) <= (e))

static std::string fixtureDir;
static std::string readFixture(const char* name) {
  std::string path = fixtureDir + "/" + name, out;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) { fprintf(stderr, "missing fixture %s\n", path.c_str()); exit(2); }
  char buf[4096]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
  fclose(f);
  return out;
}
static ModelMolecule mol;
static bool parse(const std::string& s) { return parseSdf(s.c_str(), s.size(), mol); }

static void testRefs() {
  ModelRef r;
  CHECK(parseModelRef("molecule:caffeine", r));
  CHECK(strcmp(r.kind, "molecule") == 0);
  CHECK(strcmp(r.query, "caffeine") == 0);
  CHECK(parseModelRef("molecule:sodium lauryl sulfate", r));
  CHECK(parseModelRef("globe:Matterhorn", r));
  CHECK(strcmp(r.kind, "globe") == 0);
  CHECK(parseModelRef("dino:Tyrannosaurus", r) && strcmp(r.kind, "dino") == 0);
  const char* bad[] = {"", "molecule:", "sphere:x", ":x", "caffeine", "molecule:a\nb"};
  for (const char* s : bad) {
    strcpy(r.kind, "molecule");
    CHECK(!parseModelRef(s, r));
    CHECK(r.kind[0] == 0);
  }
  CHECK(!parseModelRef(nullptr, r));
  std::string longOne = "molecule:" + std::string(64, 'a');
  CHECK(!parseModelRef(longOne.c_str(), r));
  std::string justFits = "molecule:" + std::string(63, 'a');
  CHECK(parseModelRef(justFits.c_str(), r));
}

static std::string counts(int atoms, int bonds) {
  char line[64];
  snprintf(line, sizeof line, "%3d%3d  0     0  0  0  0  0  0999 V2000\n", atoms, bonds);
  return std::string("1\n x\n\n") + line;
}
static std::string atomLine(const char* sym) {
  char line[80];
  snprintf(line, sizeof line, "%10.4f%10.4f%10.4f %-3s 0  0\n", 0.5, 0.25, -0.5, sym);
  return line;
}

static void testSdf() {
  CHECK(parse(readFixture("caffeine.sdf")));
  CHECK(mol.atomCount == 24);
  CHECK(mol.bondCount == 25);
  float maxR = 0;
  for (int i = 0; i < mol.atomCount; i++) {
    float r = sqrtf(mol.atoms[i].x * mol.atoms[i].x + mol.atoms[i].y * mol.atoms[i].y + mol.atoms[i].z * mol.atoms[i].z);
    if (r > maxR) maxR = r;
  }
  NEAR(maxR, 1.0f, 0.001f);

  std::string water = readFixture("water.sdf");
  CHECK(parse(water));
  CHECK(mol.atomCount == 3);
  CHECK(mol.bondCount == 2);
  CHECK(mol.atoms[0].element == 8);
  CHECK(mol.atoms[1].element == 1);
  CHECK(mol.bonds[0].a == 0);
  CHECK(mol.bonds[0].b == 1);
  CHECK(mol.bonds[0].order == 1);

  CHECK(!parse(""));
  CHECK(!parse("<!DOCTYPE html><html><body>Service unavailable</body></html>"));
  std::string caffeine = readFixture("caffeine.sdf");
  CHECK(!parse(caffeine.substr(0, caffeine.size() / 2)));
  CHECK(!parse(counts(0, 0) + "M  END\n"));
  CHECK(!parse(counts(201, 0)));
  CHECK(!parse(counts(1, 251) + atomLine("C")));
  CHECK(!parse(counts(2, 1) + atomLine("C") + atomLine("C") + " 1 99  1  0\nM  END\n"));
  CHECK(!parse(counts(2, 1) + atomLine("C") + atomLine("C") + " 1  1  1  0\nM  END\n"));
  CHECK(!parse(counts(1, 0) + atomLine("Xx") + "M  END\n"));
  // Non-finite or absurd coordinates are not a model
  const auto rawAtom = [](const char* x) {
    char line[80];
    snprintf(line, sizeof line, "%10s%10.4f%10.4f %-3s 0  0\n", x, 0.25, -0.5, "C");
    return std::string(line);
  };
  for (const char* x : {"nan", "inf", "-inf", "1e30"}) {
    CHECK(!parse(counts(2, 0) + rawAtom(x) + atomLine("C") + "M  END\n"));
  }
  // A single atom must not divide by zero
  CHECK(parse(counts(1, 0) + atomLine("C") + "M  END\n"));
  CHECK(mol.atoms[0].x == 0);
}

static void testMath() {
  Mat3 m; float p[3];
  mat3Identity(m);
  mat3Rotate(m, 0, 0, (float)M_PI / 2);
  rotatePoint(m, 1, 0, 0, p);
  NEAR(p[0], 0.0f, 0.01f); NEAR(p[1], 1.0f, 0.01f); NEAR(p[2], 0.0f, 0.01f);
  mat3Identity(m);
  for (int i = 0; i < 1000; i++) mat3Rotate(m, 0.03f, -0.02f, 0.05f);
  rotatePoint(m, 1, 0, 0, p);
  NEAR(sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]), 1.0f, 0.001f);
  rotatePoint(m, 0, 1, 0, p);
  NEAR(sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]), 1.0f, 0.001f);

  float z[4] = {0.5f, -1, 2, 0};
  uint8_t order[4] = {0, 1, 2, 3};
  sortByDepth(z, order, 4);
  CHECK(order[0] == 1); CHECK(order[1] == 3); CHECK(order[2] == 0); CHECK(order[3] == 2);
  sortByDepth(z, order, 4);
  CHECK(order[0] == 1); CHECK(order[3] == 2);
}

static void testElements() {
  CHECK(elementFromSymbol("C") == 6);
  CHECK(elementFromSymbol("Cl") == 17);
  CHECK(elementFromSymbol("Xx") == 0);
  CHECK(strcmp(elementSymbol(8), "O") == 0);
  CHECK(elementSymbol(0)[0] == 0);
  CHECK(elementSymbol(250)[0] == 0);
  // PubChem's 909090 for carbon, as RGB565
  CHECK(elementColor565(6) == (((0x90 >> 3) << 11) | ((0x90 >> 2) << 5) | (0x90 >> 3)));
}

static void testGyroAxes() {
  float s[3];
  // Rotation 1: the sensor's +x points up the screen and its +y to the left
  gyroToScreen(1, 10, 0, 0, s);
  CHECK(s[0] == 0); CHECK(s[1] == 10); CHECK(s[2] == 0);
  gyroToScreen(1, 0, 10, 0, s);
  CHECK(s[0] == -10); CHECK(s[1] == 0);
  gyroToScreen(1, 0, 0, 10, s);
  CHECK(s[2] == 10);
  // Rotation 3 is the same view turned by half a circle: x and y flip, the axis out of the screen doesn't
  gyroToScreen(3, 10, 0, 0, s);
  CHECK(s[1] == -10);
  gyroToScreen(3, 0, 10, 0, s);
  CHECK(s[0] == 10);
  gyroToScreen(3, 0, 0, 10, s);
  CHECK(s[2] == 10);
}

// LovyanGFX's Panel_Sprite::drawPixelPreclipped: reading coordinates to the buffer, rotation r
static void spriteToBuffer(int r, int x, int y, int bw, int bh, int& bx, int& by) {
  const int w = (r & 1) ? bh : bw, h = (r & 1) ? bw : bh;
  if ((1u << r) & 0b10010110) y = h - (y + 1);
  if (r & 2) x = w - (x + 1);
  if (r & 1) { const int t = x; x = y; y = t; }
  bx = x;
  by = y;
}

// Gravity's up in the screen's axes when the stick is turned clockwise by roll from the locked landscape
static void upAt(float roll, float out[3], float tilt = 0) {
  const float plane = cosf(tilt);
  out[0] = -sinf(roll) * plane;
  out[1] = cosf(roll) * plane;
  out[2] = sinf(tilt);
}

static void testLevel() {
  const int bw = 240, bh = 135;
  // The inverse of the sprite's own mapping, for every rotation and pixel
  int wrong = 0;
  for (int q = 0; q < 4; q++) {
    const int w = (q & 1) ? bh : bw, h = (q & 1) ? bw : bh;
    for (int y = 0; y < h; y += 7) {
      for (int x = 0; x < w; x += 5) {
        int bx, by;
        float rx, ry;
        spriteToBuffer(q, x, y, bw, bh, bx, by);
        levelToReading(q, bx, by, bw, bh, rx, ry);
        if (bx < 0 || bx >= bw || by < 0 || by >= bh || rx != x || ry != y) wrong++;
      }
    }
    // The middle of the buffer is the middle of every reading frame: the globe stays put
    float rx, ry;
    levelToReading(q, (bw - 1) / 2.0f, (bh - 1) / 2.0f, bw, bh, rx, ry);
    NEAR(rx, (w - 1) / 2.0f, 1e-4);
    NEAR(ry, (h - 1) / 2.0f, 1e-4);
  }
  CHECK(wrong == 0);

  // Held at each quarter turn: the text's up is gravity's up, and the globe's north too
  for (int k = 0; k < 4; k++) {
    const float roll = levelWrap(k * LEVEL_QUARTER);
    float up[3];
    upAt(roll, up);
    LevelState s;
    levelReset(s);
    levelStep(s, up);
    CHECK(s.known);
    NEAR(levelWrap(s.roll - roll), 0, 1e-4);
    // Reading frame: one pixel up, in the buffer (y down)
    const int q = s.quarter;
    const int w = (q & 1) ? bh : bw, h = (q & 1) ? bw : bh;
    int x0, y0, x1, y1;
    spriteToBuffer(q, w / 2, h / 2, bw, bh, x0, y0);
    spriteToBuffer(q, w / 2, h / 2 - 1, bw, bh, x1, y1);
    NEAR(x1 - x0, up[0], 1e-4);
    NEAR(y0 - y1, up[1], 1e-4);  // Buffer y runs down, gravity's up y runs up
    float m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}, d[9];
    levelMatrix(m, s.roll, d);
    NEAR(d[1], up[0], 1e-4);  // North (0, 1, 0) on screen
    NEAR(d[4], up[1], 1e-4);
  }

  // The text holds its way until the stick is 55 degrees round, then snaps; the same on the way back
  LevelState s;
  levelReset(s);
  float up[3];
  upAt(0, up);
  levelStep(s, up);
  CHECK(s.quarter == 0);
  for (int deg = 0; deg <= 50; deg += 5) {
    upAt(deg * (float)M_PI / 180, up);
    for (int i = 0; i < 40; i++) levelStep(s, up);
  }
  CHECK(s.quarter == 0);
  NEAR(s.roll, 50 * M_PI / 180, 0.01);
  upAt(60 * (float)M_PI / 180, up);
  for (int i = 0; i < 40; i++) levelStep(s, up);
  CHECK(s.quarter == 3);
  upAt(40 * (float)M_PI / 180, up);
  for (int i = 0; i < 40; i++) levelStep(s, up);
  CHECK(s.quarter == 3);
  upAt(30 * (float)M_PI / 180, up);
  for (int i = 0; i < 40; i++) levelStep(s, up);
  CHECK(s.quarter == 0);
  // Upside down, across the wrap at 180 degrees
  upAt(179 * (float)M_PI / 180, up);
  for (int i = 0; i < 60; i++) levelStep(s, up);
  CHECK(s.quarter == 2);
  upAt(-179 * (float)M_PI / 180, up);
  for (int i = 0; i < 60; i++) levelStep(s, up);
  CHECK(s.quarter == 2);
  NEAR(fabsf(s.roll), M_PI, 0.03);

  // Lying flat, gravity can't tell which way is up: the roll and the text hold
  LevelState flat;
  levelReset(flat);
  upAt(1.2f, up);
  levelStep(flat, up);
  float lying[3] = {0.05f, -0.1f, 1};
  for (int i = 0; i < 250; i++) levelStep(flat, lying);
  NEAR(flat.roll, 1.2, 1e-6);
  CHECK(flat.quarter == 3);
  // Opened lying flat: landscape as locked, until gravity says otherwise
  LevelState opened;
  levelReset(opened);
  levelStep(opened, lying);
  CHECK(!opened.known && opened.quarter == 0 && opened.roll == 0);
  // A shake (far from 1 g) doesn't swing it
  LevelState shaken;
  levelReset(shaken);
  upAt(0, up);
  levelStep(shaken, up);
  float shake[3] = {-2.5f, 0.3f, 0.2f};
  for (int i = 0; i < 10; i++) levelStep(shaken, shake);
  NEAR(shaken.roll, 0, 1e-6);
  // Tilted back 60 degrees, gravity in the plane is half: it still sets the roll, more slowly
  LevelState tilted;
  levelReset(tilted);
  upAt(0, up, 1.05f);
  levelStep(tilted, up);
  upAt(0.5f, up, 1.05f);
  for (int i = 0; i < 100; i++) levelStep(tilted, up);
  NEAR(tilted.roll, 0.5, 0.01);
  // Bad numbers change nothing
  float bad[3] = {NAN, 1, 0};
  levelStep(tilted, bad);
  NEAR(tilted.roll, 0.5, 0.01);

  // Hand motion in the upright picture's axes: a tilt about the stick's own x axis, held in portrait
  // (turned 90 degrees clockwise), is a tilt about the picture's y axis; the in-plane turn is dropped
  float g[3] = {10, 0, 7}, o[3];
  levelGyro(g, (float)M_PI / 2, o);
  NEAR(o[0], 0, 1e-4);
  NEAR(o[1], -10, 1e-4);
  CHECK(o[2] == 0);
  levelGyro(g, 0, o);
  NEAR(o[0], 10, 1e-6);
  NEAR(o[1], 0, 1e-6);
}

static void testButtonRules() {
  // Rotation 3: buttons on the left, Button C on top, Button B below
  CHECK(imageScrollDirection(3, true) == -1);
  CHECK(imageScrollDirection(3, false) == 1);
  // Rotation 1: buttons on the right, Button B on top, Button C below
  CHECK(imageScrollDirection(1, true) == 1);
  CHECK(imageScrollDirection(1, false) == -1);

  // Settings only where Button C has no other job
  CHECK(buttonCCountsForSettings(false, false, false));
  CHECK(!buttonCCountsForSettings(true, false, false));   // Lists and alerts: next
  CHECK(!buttonCCountsForSettings(false, true, false));   // Picture: scroll or switch view
  CHECK(!buttonCCountsForSettings(false, false, true));   // 3D model: switch view

  // Tabs: 3 seconds from the open or the last press, also across the millis() wrap
  CHECK(modelHintsShowing(1000, 1000));
  CHECK(modelHintsShowing(3999, 1000));
  CHECK(!modelHintsShowing(4000, 1000));
  CHECK(modelHintsShowing(500, 0xFFFFFF00UL));
  CHECK(!modelHintsShowing((uint32_t)(0xFFFFFF00UL + MODEL_HINT_MS), 0xFFFFFF00UL));
  // Turning by itself waits 3 seconds after the hand last turned; opening counts as long ago
  CHECK(!modelIdleTurnAllowed(5000, 5000));
  CHECK(!modelIdleTurnAllowed(7999, 5000));
  CHECK(modelIdleTurnAllowed(8000, 5000));
  CHECK(modelIdleTurnAllowed(100, 100 - MODEL_IDLE_AFTER_HAND_MS));
  CHECK(!modelIdleTurnAllowed(200, 0xFFFFFF00UL));
}

// The footer under a model view: 6 px per character, as if every letter were as wide
static int footerWidth(const char* t) {
  int n = 0;
  for (; *t; t++) if (((unsigned char)*t & 0xC0) != 0x80) n++;
  return 6 * n;
}

static void testFooterFit() {
  char out[160];
  // Fits whole: unchanged
  CHECK(eduFooterFit("Tokio", "Natural Earth", 228, footerWidth, out, sizeof out));
  CHECK(strcmp(out, "Tokio - Natural Earth") == 0);
  CHECK(eduFooterFit("Tokio", "", 228, footerWidth, out, sizeof out) && strcmp(out, "Tokio") == 0);
  // Too wide: the label gets shorter, the longest that fits (here 19 characters in 120 px)
  CHECK(eduFooterFit("Rio de Janeiro", "Natural Earth", 120, footerWidth, out, sizeof out));
  CHECK(strcmp(out, "R... - Natural Earth") == 0);
  CHECK(footerWidth(out) <= 120);
  CHECK(eduFooterFit("Rio de Janeiro", "Natural Earth", 150, footerWidth, out, sizeof out));
  CHECK(strcmp(out, "Rio de... - Natural Earth") == 0);
  // Never cut inside a UTF-8 character: "Zürich" keeps "Zü", not "Z" plus half a ü
  CHECK(eduFooterFit("Z\xC3\xBCrich", "Natural Earth", 6 * 21, footerWidth, out, sizeof out));
  CHECK(strcmp(out, "Z\xC3\xBC... - Natural Earth") == 0);
  // Not even one letter fits (the turned screen): the credit alone, and the caller may use the small font
  CHECK(!eduFooterFit("Z\xC3\xBCrich", "Natural Earth", 6 * 19, footerWidth, out, sizeof out));
  CHECK(strcmp(out, "Natural Earth") == 0);
  CHECK(eduIsAscii(out));
  CHECK(!eduIsAscii("Z\xC3\xBCrich"));
  // One letter fits: kept
  CHECK(eduFooterFit("Z\xC3\xBCrich", "Natural Earth", 6 * 20, footerWidth, out, sizeof out));
  CHECK(strcmp(out, "Z... - Natural Earth") == 0);
}

int main(int argc, char** argv) {
  fixtureDir = argc > 1 ? argv[1] : "fixtures";
  testRefs(); testSdf(); testMath(); testElements(); testButtonRules(); testGyroAxes(); testLevel(); testFooterFit();
  printf("%d checks, %d failures\n", checks, failures);
  if (failures == 0) printf("All model tests passed\n");
  return failures == 0 ? 0 : 1;
}
