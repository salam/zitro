// Host tests of anatomy_core.h: closed list, file check with broken and hostile input, drawing order.
// Fixtures are the real generated files in webpage2/data/anatomy/.
#include <ctype.h>
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

static bool accepts(const Bytes& b, const char* id = "heart", bool cut = false) {
  AnatomyModel m;
  return anatomyParse(b.data(), b.size(), id, cut, m);
}

// Layout of cutTetra(): the tetrahedron with a legend, flagged as cut open
static const size_t COLORS_OF_TETRA = ANATOMY_HEADER_SIZE + 24 + 24;
static const size_t LEGEND_COUNT_AT = COLORS_OF_TETRA + 4;

static Bytes cutTetra(const char* id, int lines = 2, uint8_t flags = ANATOMY_FLAG_CUT) {
  Bytes b = tetra(id, flags);
  b.resize(LEGEND_COUNT_AT + 1 + lines * ANATOMY_LEGEND_SIZE + 4, 0);
  b[LEGEND_COUNT_AT] = (uint8_t)lines;
  for (int i = 0; i < lines; i++) {
    uint8_t* line = b.data() + LEGEND_COUNT_AT + 1 + i * ANATOMY_LEGEND_SIZE;
    line[0] = i == 1 ? ANATOMY_LEGEND_NOTE : 1;
    strcpy((char*)line + 1, "Kammer");
    strcpy((char*)line + 1 + ANATOMY_LEGEND_TEXT, "Chamber");
  }
  put16(b, 10, 3);  // The last triangle is a fill, and the only one in the second colour
  b[COLORS_OF_TETRA + 1] = 0;
  b[COLORS_OF_TETRA + 3] = 1;
  fixCrc(b);
  return b;
}

static const size_t VERTS_AT = ANATOMY_HEADER_SIZE;
static const size_t TRIS_AT = VERTS_AT + 24;
static const size_t COLORS_AT = TRIS_AT + 24;
static const int ANATOMY_CUT_DRAWN_LIMIT = 625;  // Measured on the stick: 21 to 24 frames per second up to here
static const size_t ANATOMY_LEGEND_MAX_LETTERS = 16;  // The data script measures them in pixels

static void testQueries() {
  int index;
  bool cut;
  CHECK(anatomyParseQuery("heart", index, cut) && index == 0 && !cut);
  CHECK(anatomyParseQuery("Heart-Inside", index, cut) == false);  // The suffix is exact
  CHECK(anatomyParseQuery("heart-inside", index, cut) && index == 0 && cut);
  CHECK(anatomyParseQuery("HEART-inside", index, cut) && index == 0 && cut);
  // Cells always open cut open, however they're asked for
  CHECK(anatomyParseQuery("animal-cell", index, cut) && cut);
  CHECK(anatomyParseQuery("neuron-inside", index, cut) && cut);
  // No inside in any source: the question gets the picture
  for (const char* q : {"stomach-inside", "hand-inside", "skeleton-inside"}) CHECK(!anatomyParseQuery(q, index, cut) && index == -1);
  CHECK(anatomyParseQuery("stomach", index, cut) && !cut);
  for (const char* q : {"", "-inside", "heart-inside-inside", "heart-", "heart-insid", "inside", "../heart-inside", "heart-inside.bin",
                        "heart-inside ", "brain-inside/../../x", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-inside",
                        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}) {
    CHECK(!anatomyParseQuery(q, index, cut));
  }
  CHECK(!anatomyParseQuery(nullptr, index, cut));
}

static void testViews() {
  // Asked for the outside: outside, inside, text. Reopened from the text: outside again.
  AnatomyViews v = {true, true, false, false};
  anatomyViewsOpen(v);
  CHECK(!v.showingCut);
  CHECK(anatomyViewsPressC(v) && v.showingCut);
  CHECK(!anatomyViewsPressC(v));
  anatomyViewsOpen(v);
  CHECK(!v.showingCut);
  // Asked for the inside: inside, outside, text. Reopened: inside again.
  v = {true, true, true, false};
  anatomyViewsOpen(v);
  CHECK(v.showingCut);
  CHECK(anatomyViewsPressC(v) && !v.showingCut);
  CHECK(!anatomyViewsPressC(v));
  anatomyViewsOpen(v);
  CHECK(v.showingCut);
  // One view only: straight to the text, whatever failed to load
  v = {true, false, false, false};
  anatomyViewsOpen(v);
  CHECK(!v.showingCut && !anatomyViewsPressC(v) && !v.showingCut);
  v = {false, true, true, false};
  anatomyViewsOpen(v);
  CHECK(v.showingCut && !anatomyViewsPressC(v) && v.showingCut);
  v = {false, true, false, false};  // The outside was asked for and only the cut came: never happens, still safe
  anatomyViewsOpen(v);
  CHECK(v.showingCut && !anatomyViewsPressC(v));
}

static void testCutStaysInView() {
  Mat3 r, held;
  anatomyCutStart(r);
  CHECK(r.m[8] >= ANATOMY_CUT_LIMIT_COS);
  held = r;
  // The shared view spins it while the hand is still: that must not move it at all
  for (int i = 0; i < 2000; i++) {
    mat3Rotate(r, 0, 0.35f * 0.04f, 0);
    anatomyHoldCut(r, held, false);
    CHECK(fabsf(r.m[2] - held.m[2]) < 1e-6f);
  }
  Mat3 start;
  anatomyCutStart(start);
  for (int i = 0; i < 9; i++) CHECK(fabsf(r.m[i] - start.m[i]) < 1e-4f);
  // A hand turning any way, however far, never gets it past the limit, and every turn stays a turn
  for (int i = 0; i < 20000; i++) {
    mat3Rotate(r, 0.31f * sinf(i * 0.13f), 0.27f * cosf(i * 0.071f), 0.2f * sinf(i * 0.031f));
    anatomyHoldCut(r, held, true);
    CHECK(r.m[8] >= ANATOMY_CUT_LIMIT_COS - 0.002f);
    const float len = r.m[2] * r.m[2] + r.m[5] * r.m[5] + r.m[8] * r.m[8];
    CHECK(fabsf(len - 1.0f) < 0.001f);
  }
  // Facing exactly away, and exactly sideways: no division by nothing
  mat3Identity(r);
  mat3Rotate(r, 0, (float)M_PI, 0);
  anatomyClampCut(r);
  CHECK(r.m[8] >= ANATOMY_CUT_LIMIT_COS - 0.002f);
  mat3Identity(r);
  mat3Rotate(r, (float)M_PI / 2, 0, 0);
  anatomyClampCut(r);
  CHECK(r.m[8] >= ANATOMY_CUT_LIMIT_COS - 0.002f);
  CHECK(fabsf(cosf(ANATOMY_CUT_LIMIT_RAD) - ANATOMY_CUT_LIMIT_COS) < 0.0005f);
}

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
  CHECK(anatomyParse(b.data(), b.size(), "heart", false, m));
  CHECK(m.vertexCount == 4 && m.triangleCount == 4 && m.paletteCount == 2);
  CHECK(strcmp(m.labelDe, "Herz") == 0 && strcmp(m.labelEn, "Heart") == 0 && strcmp(m.source, "Test") == 0);
  CHECK(m.palette[0] == 0xF800 && m.colors[1] == 1);
  CHECK((m.flags & ANATOMY_FLAG_SCHEMATIC) == 0);
  Bytes s = cutTetra("neuron", 2, ANATOMY_FLAG_SCHEMATIC | ANATOMY_FLAG_CUT);
  CHECK(anatomyParse(s.data(), s.size(), "neuron", true, m));
  CHECK((m.flags & ANATOMY_FLAG_SCHEMATIC) != 0);
  CHECK(m.legendCount == 2 && m.frontFrom == 3);
  CHECK(anatomyLegendColor(m, 0) == 1 && anatomyLegendColor(m, 1) == ANATOMY_LEGEND_NOTE);
  CHECK(strcmp(anatomyLegendText(m, 0, true), "Kammer") == 0 && strcmp(anatomyLegendText(m, 1, false), "Chamber") == 0);
}

static void testBrokenCut() {
  const Bytes good = cutTetra("heart");
  CHECK(accepts(good, "heart", true));
  AnatomyModel m;
  for (size_t cut = 0; cut < good.size(); cut++) CHECK(!anatomyParse(good.data(), cut, "heart", true, m));
  for (size_t i = 0; i < good.size(); i += 5) { Bytes b = good; b[i] ^= 0x10; CHECK(!accepts(b, "heart", true)); }
  // The wrong view, or a view that lies about what it is: a plain file as a cut and back, and flags against the list
  CHECK(!accepts(good, "heart", false));
  CHECK(!accepts(tetra("heart"), "heart", true));
  { Bytes b = good; b[4] = 0; fixCrc(b); CHECK(!accepts(b, "heart", true)); }
  CHECK(!accepts(cutTetra("heart", 2, ANATOMY_FLAG_CUT | ANATOMY_FLAG_SCHEMATIC), "heart", true));  // An organ as a drawing
  CHECK(!accepts(cutTetra("neuron", 2, ANATOMY_FLAG_CUT), "neuron", true));                        // A drawing as data
  CHECK(!accepts(good, "brain", true));
  CHECK(!accepts(good, "hand-inside", true));
  CHECK(!accepts(good, "nothing", true));
  // First filled triangle: past the end, and in a plain file
  { Bytes b = good; put16(b, 10, 4); fixCrc(b); CHECK(!accepts(b, "heart", true)); }
  { Bytes b = good; put16(b, 10, 65535); fixCrc(b); CHECK(!accepts(b, "heart", true)); }
  { Bytes b = tetra("heart"); put16(b, 10, 1); fixCrc(b); CHECK(!accepts(b)); }
  // Legend count against the room in the file: none, too many, more or fewer than are there
  for (int count : {0, 1, 3, ANATOMY_MAX_LEGEND + 1, 255}) { Bytes b = good; b[LEGEND_COUNT_AT] = (uint8_t)count; fixCrc(b); CHECK(!accepts(b, "heart", true)); }
  CHECK(accepts(cutTetra("heart", ANATOMY_MAX_LEGEND), "heart", true));
  CHECK(!accepts(cutTetra("heart", ANATOMY_MAX_LEGEND + 1), "heart", true));
  const size_t line = LEGEND_COUNT_AT + 1;
  // Legend colour outside the palette; the note value is the only exception
  { Bytes b = good; b[line] = 2; fixCrc(b); CHECK(!accepts(b, "heart", true)); }
  { Bytes b = good; b[line] = 0xFE; fixCrc(b); CHECK(!accepts(b, "heart", true)); }
  // Legend words: unterminated, empty, control characters, anything the small font can't draw
  for (size_t at : {line + 1, line + 1 + ANATOMY_LEGEND_TEXT}) {
    { Bytes b = good; memset(b.data() + at, 'a', ANATOMY_LEGEND_TEXT); fixCrc(b); CHECK(!accepts(b, "heart", true)); }
    { Bytes b = good; b[at] = 0; fixCrc(b); CHECK(!accepts(b, "heart", true)); }
    { Bytes b = good; b[at] = 0x07; fixCrc(b); CHECK(!accepts(b, "heart", true)); }
    { Bytes b = good; b[at] = 0xC3; b[at + 1] = 0xA4; b[at + 2] = 0; fixCrc(b); CHECK(!accepts(b, "heart", true)); }
    { Bytes b = good; b[at] = 0xFF; fixCrc(b); CHECK(!accepts(b, "heart", true)); }
  }
  // Random garbage with a repaired checksum must never crash the check
  srand(7);
  for (int round = 0; round < 3000; round++) {
    Bytes b = good;
    const int changes = 1 + rand() % 12;
    for (int i = 0; i < changes; i++) b[rand() % (b.size() - 4)] = (uint8_t)rand();
    fixCrc(b);
    accepts(b, "heart", true);
    accepts(b, "heart", false);
  }
}

static void testBrokenAndHostile() {
  const Bytes good = tetra("heart");
  CHECK(accepts(good));
  AnatomyModel m;
  CHECK(!anatomyParse(nullptr, 100, "heart", false, m));
  CHECK(!anatomyParse(good.data(), good.size(), nullptr, false, m));
  CHECK(!anatomyParse(good.data(), 0, "heart", false, m));
  // Every possible cut-off download
  for (size_t cut = 0; cut < good.size(); cut++) CHECK(!anatomyParse(good.data(), cut, "heart", false, m));
  // Trailing bytes, too long, odd address
  { Bytes b = good; b.push_back(0); fixCrc(b); CHECK(!accepts(b)); }
  { Bytes b(ANATOMY_BODY_LIMIT + 1, 0); memcpy(b.data(), good.data(), good.size()); CHECK(!accepts(b)); }
  {
    Bytes b(good.size() + 2);
    memcpy(b.data() + 1, good.data(), good.size());
    const uint8_t* odd = b.data() + (((uintptr_t)b.data() & 1) ? 0 : 1);
    if ((uintptr_t)odd & 1) { memmove((void*)odd, good.data(), good.size()); CHECK(!anatomyParse(odd, good.size(), "heart", false, m)); }
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

static void testDrawing() {
  // Front-facing only: of a closed tetrahedron, at most 3 and at least 1 faces, in every turn
  Bytes b = tetra("heart");
  AnatomyModel m;
  CHECK(anatomyParse(b.data(), b.size(), "heart", false, m));
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
    CHECK(anatomyParse(two.data(), two.size(), "heart", false, pair));
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

  // A fill is drawn after everything else in the file, even when it lies further back and comes first in the sort
  {
    Bytes cutB = cutTetra("heart");
    AnatomyModel cutM;
    CHECK(anatomyParse(cutB.data(), cutB.size(), "heart", true, cutM));
    std::vector<int8_t> cutNormals(12);
    anatomyComputeNormals(cutM, cutNormals.data());
    anatomyBuildShades(cutM, shades);
    Work cutWork(cutM);
    Mat3 spin;
    mat3Identity(spin);
    int fillsSeen = 0;
    for (int turn = 0; turn < 200; turn++) {
      mat3Rotate(spin, 0.13f, 0.21f, 0.07f);
      std::vector<uint16_t> order;
      anatomyRender(cutM, cutNormals.data(), shades, spin, 50, 4, 120, 58, 240, 117, cutWork.w,
                    [&](int, int, int, int, int, int, uint16_t color) { order.push_back(color); });
      // Triangle 3 is the fill: last when it shows, and always at the same shade
      const uint16_t fill = shades[ANATOMY_SHADES + ANATOMY_FILL_SHADE];
      for (size_t i = 0; i + 1 < order.size(); i++) CHECK(order[i] != fill);
      fillsSeen += !order.empty() && order.back() == fill;
    }
    CHECK(fillsSeen > 0);
  }
}

// One view of a real file: is it what it should be, and what does the stick have to draw at most?
static void checkRealView(const std::string& dataDir, const std::string& name, const std::string& file, bool cut) {
  Bytes raw = readFile(dataDir + "/" + file);
  CHECK(!raw.empty());  // Every view on the closed list has its file
  if (raw.empty()) return;
  AnatomyModel real;
  CHECK(anatomyParse(raw.data(), raw.size(), name.c_str(), cut, real));
  if (!real.vertexCount) return;
  CHECK(real.triangleCount <= ANATOMY_MAX_TRIANGLES);
  CHECK(strlen(real.source) > 0);
  // Drawn cells must say so, and measured organs must not: this is the line between data and a drawing
  const bool isCell = ANATOMY_KINDS[anatomyFindId(name.c_str())] == ANATOMY_CUT_ONLY;
  CHECK(((real.flags & ANATOMY_FLAG_SCHEMATIC) != 0) == isCell);
  CHECK((strcmp(real.source, "Schema") == 0) == isCell);
  CHECK(cut == (real.legendCount > 0));
  for (int i = 0; i < real.legendCount; i++) {
    CHECK(strlen(anatomyLegendText(real, i, true)) <= ANATOMY_LEGEND_MAX_LETTERS);
    CHECK(strlen(anatomyLegendText(real, i, false)) <= ANATOMY_LEGEND_MAX_LETTERS);
  }
  // The words stay on the screen and clear of the amber tag
  const int legendTop = anatomyLegendTop(117, real.legendCount, isCell);
  CHECK(legendTop >= 0 && legendTop + real.legendCount * ANATOMY_LEGEND_LINE <= 117);
  if (isCell) CHECK(legendTop >= ANATOMY_TAG_HEIGHT);
  std::vector<int8_t> n(real.triangleCount * 3);
  anatomyComputeNormals(real, n.data());
  uint16_t s[ANATOMY_MAX_PALETTE * ANATOMY_SHADES];
  anatomyBuildShades(real, s);
  Work wk(real);
  Mat3 rot;
  if (cut) anatomyCutStart(rot); else mat3Identity(rot);
  int maxDrawn = 0, maxAtZoom0 = 0;
  for (int zoom = 0; zoom < 3; zoom++) {
    for (int turn = 0; turn < 240; turn++) {
      if (cut) {
        mat3Rotate(rot, 0.09f * sinf(turn * 0.37f), 0.12f * cosf(turn * 0.21f), 0.02f);
        anatomyClampCut(rot);
      } else {
        mat3Rotate(rot, 0.05f, 0.11f, 0.02f);
      }
      Mat3 shown = rot;
      if (cut) {
        mat3Rotate(shown, 0, ANATOMY_CUT_SWAY_RAD * sinf(turn * 0.5f), 0);
        anatomyClampCut(shown);
      }
      int calls = 0;
      bool onScreenSizes = true;
      const uint16_t drawn = anatomyRender(real, n.data(), s, shown, anatomyViewScaleFor(117, real.legendCount, zoom), ANATOMY_VIEW_CAMERA,
                                           anatomyViewCentreX(240, real.legendCount, zoom), 58, 240, 117, wk.w,
                                           [&](int a, int c, int d, int e, int f, int g, uint16_t) {
                                             calls++;
                                             if (abs(a) > 2000 || abs(c) > 2000 || abs(d) > 2000 || abs(e) > 2000 || abs(f) > 2000 || abs(g) > 2000) onScreenSizes = false;
                                           });
      CHECK(drawn == calls);
      CHECK(drawn > 0);
      CHECK(onScreenSizes);
      if (drawn > maxDrawn) maxDrawn = drawn;
      if (zoom == 0 && drawn > maxAtZoom0) maxAtZoom0 = drawn;
    }
  }
  // At the size the stick was measured to hold 21 to 24 frames per second
  if (cut) CHECK(maxDrawn <= ANATOMY_CUT_DRAWN_LIMIT);
  printf("  %-20s %5zu bytes, %4d triangles, %3d drawn at most\n", file.c_str(), raw.size(), real.triangleCount, maxDrawn);
}

static void testRealFiles(const std::string& dataDir) {
  for (int id = 0; id < ANATOMY_ID_COUNT; id++) {
    const std::string name = ANATOMY_IDS[id];
    const AnatomyKind kind = ANATOMY_KINDS[id];
    if (kind == ANATOMY_CUT_ONLY) {
      checkRealView(dataDir, name, name + ".bin", true);
      continue;
    }
    checkRealView(dataDir, name, name + ".bin", false);
    if (kind == ANATOMY_OUTSIDE_AND_CUT) checkRealView(dataDir, name, name + "-cut.bin", true);
    // No file promises an inside that the closed list doesn't offer
    if (kind == ANATOMY_OUTSIDE_ONLY) CHECK(readFile(dataDir + "/" + name + "-cut.bin").empty());
  }
}

// The audience includes small children: no part with one of these names may be in any file
static void testNothingUnsuitable(const std::string& dataDir) {
  Bytes raw = readFile(dataDir + "/index.json");
  CHECK(!raw.empty());
  std::string index(raw.begin(), raw.end());
  // An empty index would pass the check below without looking at anything
  for (int id = 0; id < ANATOMY_ID_COUNT; id++) {
    CHECK(index.find(std::string("\"id\": \"") + ANATOMY_IDS[id] + "\"") != std::string::npos);
  }
  CHECK(index.find("\"parts\": []") == std::string::npos);
  for (const char* credit : {"The Database Center for Life Science", "Reference Organ for Lung", "Reference Organ for Heart", "Reference Organ for Kidney",
                             "Reference Organ for Eye", "Kristen Browne", "Heidi Schlehlein", "Not measured"}) {
    CHECK(index.find(credit) != std::string::npos);
  }
  // Every cutaway is in the index, says where it's from, and names what it shows
  for (int id = 0; id < ANATOMY_ID_COUNT; id++) {
    if (ANATOMY_KINDS[id] != ANATOMY_OUTSIDE_AND_CUT) continue;
    CHECK(index.find(std::string("\"file\": \"") + ANATOMY_IDS[id] + "-cut.bin\"") != std::string::npos);
  }
  for (char& c : index) c = (char)tolower((unsigned char)c);
  for (const char* no : {"penis", "testis", "prostate", "scrot", "semin", "epididym", "uterus", "uterine", "ovar", "vagin", "vulv",
                         "breast", "mammary", "skin", "nipple", "pubic hair"}) {
    CHECK(index.find(no) == std::string::npos);
  }
}

int main(int argc, char** argv) {
  testClosedList();
  testQueries();
  testViews();
  testCutStaysInView();
  testValidFile();
  testBrokenAndHostile();
  testBrokenCut();
  testDrawing();
  const std::string dataDir = argc > 1 ? argv[1] : "../../webpage2/data/anatomy";
  testRealFiles(dataDir);
  testNothingUnsuitable(dataDir);
  if (failures) {
    printf("%d failure(s)\n", failures);
    return 1;
  }
  printf("anatomy tests passed\n");
  return 0;
}
