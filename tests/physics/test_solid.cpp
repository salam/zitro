// Host tests for the geometric solids. Build and run: tests/physics/run.sh
#include <map>
#include <set>
#include <string>
#include <utility>
#include "../../sketch/sketch_jan12a/solid_core.h"
#include "test_common.h"

static SolidMesh mesh;

static int idIndex(const char* id) {
  const int i = solidFindId(id);
  if (i < 0) fprintf(stderr, "  unknown id %s\n", id);
  return i;
}

static void testCounts() {
  struct { const char* id; int v, e, f; } known[] = {
      {"tetrahedron", 4, 6, 4},      {"cube", 8, 12, 6},           {"octahedron", 6, 12, 8},
      {"dodecahedron", 20, 30, 12},  {"icosahedron", 12, 30, 20},  {"cuboid", 8, 12, 6},
      {"triangular-prism", 6, 9, 5}, {"pentagonal-prism", 10, 15, 7}, {"hexagonal-prism", 12, 18, 8},
      {"triangular-pyramid", 4, 6, 4}, {"square-pyramid", 5, 8, 5},
      {"pentagonal-pyramid", 6, 10, 6}, {"hexagonal-pyramid", 7, 12, 7},
  };
  for (auto& k : known) {
    CHECK(solidBuild(idIndex(k.id), mesh));
    CHECK(!mesh.curved);
    if (mesh.vertices != k.v || mesh.edges != k.e || mesh.faces != k.f) {
      fprintf(stderr, "  %s: V%d E%d F%d\n", k.id, mesh.vertices, mesh.edges, mesh.faces);
    }
    CHECK(mesh.vertices == k.v);
    CHECK(mesh.edges == k.e);
    CHECK(mesh.faces == k.f);
  }
}

static void testEulerAndCurved() {
  int polyhedra = 0, curved = 0;
  for (int i = 0; i < SOLID_TABLE_SIZE; i++) {
    CHECK(solidBuild(i, mesh));
    if (mesh.curved) {
      curved++;
      CHECK(mesh.faces == 0 && mesh.edges == 0 && mesh.vertices == 0);  // No counts on curved solids
    } else {
      polyhedra++;
      CHECK(mesh.vertices - mesh.edges + mesh.faces == 2);  // Euler
    }
  }
  CHECK(polyhedra == 13);
  CHECK(curved == 4);
}

// Every edge used once in each direction: the surface is closed and the faces agree on outside
static void testClosedSurfaces() {
  for (int i = 0; i < SOLID_TABLE_SIZE; i++) {
    CHECK(solidBuild(i, mesh));
    std::map<std::pair<int, int>, int> directed;
    for (int k = 0; k < mesh.faceCount; k++) {
      const SolidFace& f = mesh.f[k];
      for (int c = 0; c < f.count; c++) {
        directed[{mesh.indices[f.start + c], mesh.indices[f.start + (c + 1) % f.count]}]++;
      }
    }
    bool closed = true;
    for (auto& e : directed) {
      auto back = directed.find({e.first.second, e.first.first});
      if (e.second != 1 || back == directed.end() || back->second != 1) closed = false;
    }
    if (!closed) fprintf(stderr, "  not closed: %s\n", SOLID_TABLE[i].id);
    CHECK(closed);
  }
}

// Faces are flat, face outward, and everything is scaled into the unit sphere
static void testFacesAndScale() {
  for (int i = 0; i < SOLID_TABLE_SIZE; i++) {
    CHECK(solidBuild(i, mesh));
    CHECK(mesh.vertexCount <= SOLID_MAX_VERTICES);
    CHECK(mesh.faceCount < 256);
    float maxR = 0;
    for (int k = 0; k < mesh.vertexCount; k++) {
      const float* p = mesh.v[k];
      maxR = fmaxf(maxR, sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]));
    }
    NEAR(maxR, 1.0, 0.001);
    const bool torus = SOLID_TABLE[i].shape == SOLID_TORUS;
    bool flat = true, outward = true;
    for (int k = 0; k < mesh.faceCount; k++) {
      const SolidFace& f = mesh.f[k];
      NEAR(f.nx * f.nx + f.ny * f.ny + f.nz * f.nz, 1.0, 0.001);
      for (int c = 0; c < f.count; c++) {
        const float* p = mesh.v[mesh.indices[f.start + c]];
        if (fabsf((p[0] - f.cx) * f.nx + (p[1] - f.cy) * f.ny + (p[2] - f.cz) * f.nz) > 0.002f) flat = false;
      }
      float ox = f.cx, oy = f.cy, oz = f.cz;
      if (torus) {  // Outward from the tube's centre line, not from the middle of the ring
        const float r = sqrtf(ox * ox + oz * oz);
        ox -= 0.68f * ox / r;
        oz -= 0.68f * oz / r;
      }
      if (ox * f.nx + oy * f.ny + oz * f.nz <= 0) outward = false;
    }
    if (!flat || !outward) fprintf(stderr, "  %s flat=%d outward=%d\n", SOLID_TABLE[i].id, flat, outward);
    CHECK(flat);
    CHECK(outward);
  }
}

// In a Platonic solid every edge is equally long and every face has the same number of corners
static void testPlatonic() {
  struct { const char* id; int corners; } solids[] = {
      {"tetrahedron", 3}, {"cube", 4}, {"octahedron", 3}, {"dodecahedron", 5}, {"icosahedron", 3}};
  for (auto& s : solids) {
    CHECK(solidBuild(idIndex(s.id), mesh));
    float low = 99, high = 0;
    for (int k = 0; k < mesh.faceCount; k++) {
      const SolidFace& f = mesh.f[k];
      CHECK(f.count == s.corners);
      for (int c = 0; c < f.count; c++) {
        const float* a = mesh.v[mesh.indices[f.start + c]];
        const float* b = mesh.v[mesh.indices[f.start + (c + 1) % f.count]];
        const float d = sqrtf(powf(a[0] - b[0], 2) + powf(a[1] - b[1], 2) + powf(a[2] - b[2], 2));
        low = fminf(low, d);
        high = fmaxf(high, d);
      }
    }
    NEAR(low, high, 0.001);
  }
  // Prisms have square walls: every edge as long as the base edge
  CHECK(solidBuild(idIndex("hexagonal-prism"), mesh));
  float low = 99, high = 0;
  for (int k = 0; k < mesh.faceCount; k++) {
    const SolidFace& f = mesh.f[k];
    for (int c = 0; c < f.count; c++) {
      const float* a = mesh.v[mesh.indices[f.start + c]];
      const float* b = mesh.v[mesh.indices[f.start + (c + 1) % f.count]];
      const float d = sqrtf(powf(a[0] - b[0], 2) + powf(a[1] - b[1], 2) + powf(a[2] - b[2], 2));
      low = fminf(low, d);
      high = fmaxf(high, d);
    }
  }
  NEAR(low, high, 0.001);
}

static void testIds() {
  CHECK(solidFindId("cube") >= 0);
  CHECK(solidFindId("CUBE") == solidFindId("cube"));
  CHECK(solidFindId("Hexagonal Prism") == solidFindId("hexagonal-prism"));
  CHECK(solidFindId("square_pyramid") == solidFindId("square-pyramid"));
  const char* bad[] = {"", " ", "cub", "cubes", "cube ", "cube;drop", "../cube", "%63ube", ("W\xC3\xBC" "rfel"),
                       "a very long name that is not a solid at all", "\xFF\xFE", "hypercube"};
  for (const char* s : bad) CHECK(solidFindId(s) == -1);
  CHECK(solidFindId(nullptr) == -1);
  std::string huge(100000, 'a');
  CHECK(solidFindId(huge.c_str()) == -1);
  CHECK(!solidBuild(-1, mesh));
  CHECK(!solidBuild(SOLID_TABLE_SIZE, mesh));
  CHECK(!solidBuild(32767, mesh));
}

// The closed list in the prompt and the table are the same, in both directions
static void testPromptRule() {
  const std::string rule = SOLID_PROMPT_RULE;
  for (char c : rule) CHECK((unsigned char)c < 0x80);  // Plain ASCII
  const std::string marker = "id genau eine von: ";
  const size_t start = rule.find(marker);
  CHECK(start != std::string::npos);
  const size_t end = rule.find('.', start);
  std::set<std::string> listed;
  std::string word;
  for (size_t i = start + marker.size(); i <= end; i++) {
    if (rule[i] == ' ' || rule[i] == '.') {
      if (!word.empty()) listed.insert(word);
      word.clear();
    } else {
      word += rule[i];
    }
  }
  CHECK((int)listed.size() == SOLID_TABLE_SIZE);
  for (const std::string& id : listed) CHECK(solidFindId(id.c_str()) >= 0);
  for (int i = 0; i < SOLID_TABLE_SIZE; i++) CHECK(listed.count(SOLID_TABLE[i].id) == 1);
  // The plain words a child uses have a default, or they'd miss the list and give a photo
  CHECK(rule.find("Pyramide square-pyramid") != std::string::npos);
  CHECK(rule.find("Prisma triangular-prism") != std::string::npos);
}

static void testShading() {
  CHECK(solidShade565(0xFFFF, 1.0f) == 0xFFFF);
  CHECK(solidShade565(0xFFFF, 0.0f) == 0);
  CHECK(solidShade565(0xF800, 5.0f) == 0xF800);
  CHECK(solidShade565(0xF800, -1.0f) == 0);
  NEAR(solidBrightness(0, 0, -1), 0.3, 0.001);  // Turned away: only the light from everywhere
  NEAR(solidBrightness(-0.40825f, 0.40825f, 0.8165f), 1.0, 0.001);  // Facing the lamp
}

int main() {
  testCounts();
  testEulerAndCurved();
  testClosedSurfaces();
  testFacesAndScale();
  testPlatonic();
  testIds();
  testPromptRule();
  testShading();
  return finish("solid");
}
