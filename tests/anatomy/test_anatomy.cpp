// Host tests of anatomy_core.h: closed list, file check with broken and hostile input, drawing order.
// Fixtures are the real generated files in webpage2/data/anatomy/.
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "../../sketch/sketch_jan12a/anatomy_core.h"
#include "../../sketch/sketch_jan12a/anatomy_view.h"

static int failures = 0;
#define CHECK(cond)                                         \
  do {                                                      \
    if (!(cond)) {                                          \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                           \
    }                                                       \
  } while (0)

typedef std::vector<uint8_t> Bytes;

static Bytes readFile(const std::string& path) {
  Bytes out;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return out;
  uint8_t buffer[4096];
  size_t n;
  while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) out.insert(out.end(), buffer, buffer + n);
  fclose(f);
  return out;
}

static void put16(Bytes& b, size_t at, uint16_t v) {
  b[at] = (uint8_t)v;
  b[at + 1] = (uint8_t)(v >> 8);
}

// A hostile file has a correct checksum: the CRC must not be the only thing standing in the way
static void fixCrc(Bytes& b) {
  const uint32_t crc = anatomyCrc32(b.data(), b.size() - 4);
  memcpy(b.data() + b.size() - 4, &crc, 4);
}

// A small valid file: a tetrahedron
static Bytes tetra(const char* id, uint8_t flags = 0) {
  Bytes b(ANATOMY_HEADER_SIZE + 4 * 6 + 4 * 6 + 4 + 4, 0);
  memcpy(b.data(), "ZAN1", 4);
  b[4] = flags;
  b[5] = 2;
  put16(b, 6, 4);
  put16(b, 8, 4);
  strcpy((char*)b.data() + 12, id);
  strcpy((char*)b.data() + 28, "Herz");
  strcpy((char*)b.data() + 52, "Heart");
  strcpy((char*)b.data() + 76, "Test");
  put16(b, 100, 0xF800);
  put16(b, 102, 0x07E0);
  const int16_t s = 18000;
  const int16_t v[12] = {s, s, s, (int16_t)-s, (int16_t)-s, s, (int16_t)-s, s, (int16_t)-s, s, (int16_t)-s, (int16_t)-s};
  memcpy(b.data() + ANATOMY_HEADER_SIZE, v, sizeof(v));
  const uint16_t t[12] = {0, 1, 3, 0, 2, 1, 0, 3, 2, 1, 2, 3};
  memcpy(b.data() + ANATOMY_HEADER_SIZE + 24, t, sizeof(t));
  b[ANATOMY_HEADER_SIZE + 24 + 24 + 1] = 1;
  fixCrc(b);
  return b;
}

static bool accepts(const Bytes& b, const char* id = "heart") {
  AnatomyModel m;
  return anatomyParse(b.data(), b.size(), id, m);
}

static const size_t VERTS_AT = ANATOMY_HEADER_SIZE;
static const size_t TRIS_AT = VERTS_AT + 24;
static const size_t COLORS_AT = TRIS_AT + 24;

static void testClosedList() {
  CHECK(anatomyFindId("heart") == 0);
  CHECK(anatomyFindId("Heart") == 0);
  CHECK(anatomyFindId("neuron") == ANATOMY_ID_COUNT - 1);
  CHECK(anatomyFindId("") == -1);
  CHECK(anatomyFindId(nullptr) == -1);
  CHECK(anatomyFindId("hear") == -1);
  CHECK(anatomyFindId("hearts") == -1);
  CHECK(anatomyFindId("../heart") == -1);
  CHECK(anatomyFindId("heart/../../secret") == -1);
  CHECK(anatomyFindId("heart.bin") == -1);
  CHECK(anatomyFindId("heart ") == -1);
  // Never on the list, whatever is in the source data
  for (const char* no : {"testis", "penis", "prostate", "uterus", "ovary", "breast", "body", "skin", "nude"}) {
    CHECK(anatomyFindId(no) == -1);
  }
}

static void testValidFile() {
  Bytes b = tetra("heart");
  AnatomyModel m;
  CHECK(anatomyParse(b.data(), b.size(), "heart", m));
  CHECK(m.vertexCount == 4 && m.triangleCount == 4 && m.paletteCount == 2);
  CHECK(strcmp(m.labelDe, "Herz") == 0 && strcmp(m.labelEn, "Heart") == 0 && strcmp(m.source, "Test") == 0);
  CHECK(m.palette[0] == 0xF800 && m.colors[1] == 1);
  CHECK((m.flags & ANATOMY_FLAG_SCHEMATIC) == 0);
  Bytes s = tetra("neuron", ANATOMY_FLAG_SCHEMATIC);
  CHECK(anatomyParse(s.data(), s.size(), "neuron", m));
  CHECK((m.flags & ANATOMY_FLAG_SCHEMATIC) != 0);
}

static void testBrokenAndHostile() {
  const Bytes good = tetra("heart");
  CHECK(accepts(good));
  AnatomyModel m;
  CHECK(!anatomyParse(nullptr, 100, "heart", m));
  CHECK(!anatomyParse(good.data(), good.size(), nullptr, m));
  CHECK(!anatomyParse(good.data(), 0, "heart", m));
  // Every possible cut-off download
  for (size_t cut = 0; cut < good.size(); cut++) CHECK(!anatomyParse(good.data(), cut, "heart", m));
  // Trailing bytes, too long, odd address
  { Bytes b = good; b.push_back(0); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b(ANATOMY_BODY_LIMIT + 1, 0); memcpy(b.data(), good.data(), good.size()); CHECK(!accepts(b)); }
  {
    Bytes b(good.size() + 2);
    memcpy(b.data() + 1, good.data(), good.size());
    const uint8_t* odd = b.data() + (((uintptr_t)b.data() & 1) ? 0 : 1);
    if ((uintptr_t)odd & 1) { memmove((void*)odd, good.data(), good.size()); CHECK(!anatomyParse(odd, good.size(), "heart", m)); }
  }
  // Every single flipped bit without a repaired checksum
  for (size_t i = 0; i < good.size(); i += 7) { Bytes b = good; b[i] ^= 0x10; CHECK(!accepts(b)); }
  // Wrong identity
  CHECK(!accepts(good, "brain"));
  CHECK(!accepts(good, "hear"));
  { Bytes b = good; b[0] = 'X'; fixCrc(b); CHECK(!accepts(b)); }
  // Header fields, with a correct checksum
  { Bytes b = good; b[4] = 0x80; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; b[5] = 0; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; b[5] = 9; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; b[5] = 255; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; put16(b, 6, 0); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; put16(b, 6, 2); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; put16(b, 6, 65535); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; put16(b, 6, ANATOMY_MAX_VERTICES + 1); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; put16(b, 8, 0); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; put16(b, 8, 65535); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; put16(b, 8, ANATOMY_MAX_TRIANGLES + 1); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; put16(b, 8, 5); fixCrc(b); CHECK(!accepts(b)); }  // Claims more than the file holds
  { Bytes b = good; put16(b, 8, 3); fixCrc(b); CHECK(!accepts(b)); }  // Claims less
  { Bytes b = good; b[10] = 1; fixCrc(b); CHECK(!accepts(b)); }
  // Text fields: unterminated, empty, control characters, broken UTF-8
  { Bytes b = good; memset(b.data() + 12, 'a', 16); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; memset(b.data() + 28, 'a', 24); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; memset(b.data() + 52, 'a', 24); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; memset(b.data() + 76, 'a', 24); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; b[28] = 0; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; b[76] = 0; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; b[28] = 0x07; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; b[52] = 0xFF; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; b[28] = 0xC3; b[29] = 0; fixCrc(b); CHECK(!accepts(b)); }  // Lead byte cut off
  { Bytes b = good; b[28] = 0xC3; b[29] = 0x41; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; b[28] = 0xF0; b[29] = 0x9F; b[30] = 0x98; b[31] = 0x80; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; b[28] = 0xC3; b[29] = 0xA4; b[30] = 0; fixCrc(b); CHECK(accepts(b)); }  // A real umlaut is fine
  // Indices, colours, coordinates
  const uint16_t badIndices[] = {4, 5, 2499, 65535};
  for (uint16_t bad : badIndices) {
    for (int pos = 0; pos < 12; pos++) { Bytes b = good; put16(b, TRIS_AT + pos * 2, bad); fixCrc(b); CHECK(!accepts(b)); }
  }
  { Bytes b = good; put16(b, TRIS_AT, 1); fixCrc(b); CHECK(!accepts(b)); }  // Degenerate: a == b
  { Bytes b = good; b[COLORS_AT] = 2; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; b[COLORS_AT + 3] = 255; fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; put16(b, VERTS_AT, 32767); put16(b, VERTS_AT + 2, 32767); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b = good; put16(b, VERTS_AT, 0x8000); put16(b, VERTS_AT + 2, 0x8000); put16(b, VERTS_AT + 4, 0x8000); fixCrc(b); CHECK(!accepts(b)); }
}

struct Work {
  std::vector<int16_t> sx, sy, depth;
  std::vector<uint16_t> next, heads;
  AnatomyWork w;
  explicit Work(const AnatomyModel& m)
      : sx(m.vertexCount), sy(m.vertexCount), depth(m.vertexCount), next(m.triangleCount), heads(ANATOMY_DEPTH_BUCKETS) {
    w = {sx.data(), sy.data(), depth.data(), next.data(), heads.data()};
  }
};

static void testDrawing(const std::string& dataDir) {
  // Front-facing only: of a closed tetrahedron, at most 3 and at least 1 faces, in every turn
  Bytes b = tetra("heart");
  AnatomyModel m;
  CHECK(anatomyParse(b.data(), b.size(), "heart", m));
  std::vector<int8_t> normals(m.triangleCount * 3);
  anatomyComputeNormals(m, normals.data());
  uint16_t shades[ANATOMY_MAX_PALETTE * ANATOMY_SHADES];
  anatomyBuildShades(m, shades);
  CHECK(shades[ANATOMY_SHADES - 1] != 0);
  // Full light gives the palette colour, shadow stays visibly darker but not black
  CHECK(shades[ANATOMY_SHADES - 1] == 0xF800);
  CHECK(shades[0] != 0 && shades[0] < 0xF800);
  Work work(m);
  Mat3 r;
  mat3Identity(r);
  for (int turn = 0; turn < 200; turn++) {
    mat3Rotate(r, 0.13f, 0.21f, 0.07f);
    int calls = 0;
    const uint16_t drawn = anatomyRender(m, normals.data(), shades, r, 50, 4, 120, 58, 240, 117, work.w,
                                         [&](int, int, int, int, int, int, uint16_t) { calls++; });
    CHECK(drawn == calls);
    CHECK(drawn >= 1 && drawn <= 3);
  }

  // Far to near: of two faces turned to the viewer, the nearer one is drawn last, whichever comes first in the file
  for (int nearFirst = 0; nearFirst < 2; nearFirst++) {
    Bytes two(ANATOMY_HEADER_SIZE + 6 * 6 + 2 * 6 + 2 + 4, 0);
    memcpy(two.data(), "ZAN1", 4);
    two[5] = 2;
    put16(two, 6, 6);
    put16(two, 8, 2);
    strcpy((char*)two.data() + 12, "heart");
    strcpy((char*)two.data() + 28, "A");
    strcpy((char*)two.data() + 52, "A");
    strcpy((char*)two.data() + 76, "A");
    put16(two, 100, 0xF800);
    put16(two, 102, 0x07E0);
    const int16_t zFirst = nearFirst ? 12000 : -12000;
    const int16_t zSecond = (int16_t)-zFirst;
    const int16_t v[18] = {-9000, -9000, zFirst, 9000, -9000, zFirst, 0, 9000, zFirst,
                           -9000, -9000, zSecond, 9000, -9000, zSecond, 0, 9000, zSecond};
    memcpy(two.data() + ANATOMY_HEADER_SIZE, v, sizeof(v));
    const uint16_t t[6] = {0, 1, 2, 3, 4, 5};
    memcpy(two.data() + ANATOMY_HEADER_SIZE + 36, t, sizeof(t));
    two[ANATOMY_HEADER_SIZE + 36 + 12 + 1] = 1;
    fixCrc(two);
    AnatomyModel pair;
    CHECK(anatomyParse(two.data(), two.size(), "heart", pair));
    std::vector<int8_t> pairNormals(6);
    anatomyComputeNormals(pair, pairNormals.data());
    anatomyBuildShades(pair, shades);
    Work pairWork(pair);
    Mat3 still;
    mat3Identity(still);
    std::vector<uint16_t> colors;
    const uint16_t drawn = anatomyRender(pair, pairNormals.data(), shades, still, 50, 4, 120, 58, 240, 117, pairWork.w,
                                         [&](int, int, int, int, int, int, uint16_t color) { colors.push_back(color); });
    CHECK(drawn == 2);  // Counter-clockwise from outside means turned to the viewer
    CHECK(colors.size() == 2);
    if (colors.size() == 2) {
      // Green is the second face in the file
      const bool greenLast = (colors[1] & 0x07E0) != 0;
      CHECK(greenLast == (nearFirst == 0));
    }
  }

  // The real files, all the way round at every zoom
  for (int id = 0; id < ANATOMY_ID_COUNT; id++) {
    const std::string name = ANATOMY_IDS[id];
    Bytes file = readFile(dataDir + "/" + name + ".bin");
    if (file.empty()) continue;
    printf("  real file %s: %zu bytes\n", name.c_str(), file.size());
    AnatomyModel real;
    CHECK(anatomyParse(file.data(), file.size(), name.c_str(), real));
    if (!real.vertexCount) continue;
    CHECK(real.triangleCount <= ANATOMY_MAX_TRIANGLES);
    // No model on the list ships with a nameless source; schematics say so
    CHECK(strlen(real.source) > 0);
    std::vector<int8_t> n(real.triangleCount * 3);
    anatomyComputeNormals(real, n.data());
    uint16_t s[ANATOMY_MAX_PALETTE * ANATOMY_SHADES];
    anatomyBuildShades(real, s);
    Work wk(real);
    Mat3 rot;
    mat3Identity(rot);
    int maxDrawn = 0;
    for (int zoom = 0; zoom < 3; zoom++) {
      for (int turn = 0; turn < 60; turn++) {
        mat3Rotate(rot, 0.05f, 0.11f, 0.02f);
        int calls = 0;
        bool onScreenSizes = true;
        const uint16_t drawn = anatomyRender(real, n.data(), s, rot, anatomyViewScale(117, zoom), ANATOMY_VIEW_CAMERA, 120, 58, 240, 117, wk.w,
                                             [&](int a, int c, int d, int e, int f, int g, uint16_t) {
                                               calls++;
                                               if (abs(a) > 2000 || abs(c) > 2000 || abs(d) > 2000 || abs(e) > 2000 || abs(f) > 2000 || abs(g) > 2000) onScreenSizes = false;
                                             });
        CHECK(drawn == calls);
        CHECK(drawn > 0);
        CHECK(onScreenSizes);
        if (drawn > maxDrawn) maxDrawn = drawn;
      }
    }
    printf("    %d triangles, at most %d drawn in a frame\n", real.triangleCount, maxDrawn);
  }
}

int main(int argc, char** argv) {
  testClosedList();
  testValidFile();
  testBrokenAndHostile();
  testDrawing(argc > 1 ? argv[1] : "../../webpage2/data/anatomy");
  if (failures) {
    printf("%d failure(s)\n", failures);
    return 1;
  }
  printf("anatomy tests passed\n");
  return 0;
}
