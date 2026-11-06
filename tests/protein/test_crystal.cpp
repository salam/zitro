// Host tests for the crystal view's pure logic, run by tests/protein/run.sh
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../sketch/sketch_jan12a/crystal_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, e) CHECK(fabs((a) - (b)) <= (e))

static std::vector<CrystalAtom> atoms(CRYSTAL_MAX_ATOMS);
static std::vector<uint16_t> adjStart(CRYSTAL_MAX_ATOMS + 1);
static std::vector<uint16_t> adj(CRYSTAL_MAX_BONDS * 2);
static uint16_t nAtoms, nBonds;
static float perUnit;

static bool build(const CrystalEntry& e) {
  return crystalBuild(e, atoms.data(), CRYSTAL_MAX_ATOMS, adjStart.data(), adj.data(), CRYSTAL_MAX_BONDS * 2, nAtoms, nBonds, perUnit);
}
static const CrystalEntry& get(const char* query) {
  const int i = crystalResolve(query);
  if (i < 0) { fprintf(stderr, "missing crystal %s\n", query); exit(2); }
  return CRYSTAL_ENTRIES[i];
}
static float distance(int i, int j) {
  const float dx = atoms[i].x - atoms[j].x, dy = atoms[i].y - atoms[j].y, dz = atoms[i].z - atoms[j].z;
  return sqrtf(dx * dx + dy * dy + dz * dz) * perUnit;
}
// Shortest distance between two elements in the built block, in Angstrom
static float shortest(uint8_t a, uint8_t b) {
  float best = 1e9f;
  for (int i = 0; i < nAtoms; i++) {
    for (int j = 0; j < nAtoms; j++) {
      if (i != j && atoms[i].element == a && atoms[j].element == b) best = std::min(best, distance(i, j));
    }
  }
  return best;
}
static int countElement(uint8_t e) {
  int n = 0;
  for (int i = 0; i < nAtoms; i++) n += atoms[i].element == e;
  return n;
}

static void testResolve() {
  const char* closed[] = {"salt", "diamond", "ice", "graphite", "quartz", "calcite", "pyrite", "fluorite", "copper",
                          "gold", "silver", "iron", "silicon"};
  for (const char* key : closed) {
    const int i = crystalResolve(key);
    CHECK(i >= 0);
    if (i >= 0) CHECK(!strcmp(CRYSTAL_ENTRIES[i].key, key));
  }
  CHECK(!strcmp(get("Table salt").key, "salt"));
  CHECK(!strcmp(get("sodium chloride").key, "salt"));
  CHECK(!strcmp(get("Kochsalz").key, "salt"));
  CHECK(!strcmp(get("DIAMANT").key, "diamond"));
  CHECK(!strcmp(get("fools gold").key, "pyrite"));
  const char* unknown[] = {"", "   ", "crystal", "sugar", "emerald", "sal", "saltt", "salt;diamond", "1000041",
                           "COD 1000041", "NaCl2", "\xC3\xA9", "salt\n"};
  for (const char* q : unknown) CHECK(crystalResolve(q) < 0);
  CHECK(crystalResolve(nullptr) < 0);
}

// Every entry in the list fits the limits, with no name shared and a real source
static void testEveryEntry() {
  std::map<std::string, int> names;
  CHECK(CRYSTAL_COUNT >= 4);
  for (int i = 0; i < CRYSTAL_COUNT; i++) {
    const CrystalEntry& e = CRYSTAL_ENTRIES[i];
    CHECK(build(e));
    CHECK(nAtoms >= 8 && nAtoms <= CRYSTAL_MAX_ATOMS);
    CHECK(nBonds >= 1 && nBonds <= CRYSTAL_MAX_BONDS);
    CHECK(!strncmp(e.source, "COD ", 4) && strlen(e.source) == 11);
    for (int k = 0; k < e.siteCount; k++) {
      const CrystalSite& site = e.sites[k];
      CHECK(site.x >= 0 && site.x < 1 && site.y >= 0 && site.y < 1 && site.z >= 0 && site.z < 1);
    }
    // No atom twice, and every one has a place: the block is centred, the farthest at radius 1
    float maxR = 0, sx = 0, sy = 0, sz = 0;
    for (int a = 0; a < nAtoms; a++) {
      maxR = std::max(maxR, sqrtf(atoms[a].x * atoms[a].x + atoms[a].y * atoms[a].y + atoms[a].z * atoms[a].z));
      sx += atoms[a].x; sy += atoms[a].y; sz += atoms[a].z;
      CHECK(atoms[a].element > 0 && atoms[a].element < ATOM_TABLE_SIZE);
      for (int b = a + 1; b < nAtoms; b++) CHECK(distance(a, b) > 0.5f);
    }
    NEAR(maxR, 1.0f, 1e-3f);
    NEAR(sx / nAtoms, 0, 1e-3f); NEAR(sy / nAtoms, 0, 1e-3f); NEAR(sz / nAtoms, 0, 1e-3f);
    // Bonds go both ways, are within range, and their count adds up
    CHECK(adjStart[0] == 0);
    CHECK(adjStart[nAtoms] == nBonds * 2);
    std::set<std::pair<int, int>> seen;
    for (int a = 0; a < nAtoms; a++) {
      CHECK(adjStart[a] <= adjStart[a + 1]);
      for (int k = adjStart[a]; k < adjStart[a + 1]; k++) {
        const int b = adj[k];
        CHECK(b >= 0 && b < nAtoms && b != a);
        CHECK(seen.insert({a, b}).second);
        CHECK(distance(a, b) < 3.05f);
      }
    }
    for (const auto& p : seen) CHECK(seen.count({p.second, p.first}) == 1);
    // Labels fit the bottom line, in the small font, next to the source and a legend
    CHECK(strlen(e.de) + strlen(e.source) <= 30 && strlen(e.en) + strlen(e.source) <= 30);
    const char* p = e.names;
    while (*p) {
      const char* semi = strchr(p, ';');
      std::string name = semi ? std::string(p, semi - p) : std::string(p);
      char normal[PROTEIN_NORMALISED_SIZE];
      CHECK(proteinNormalise(name.c_str(), normal, sizeof normal));
      CHECK(crystalResolve(name.c_str()) == i);  // No name means two crystals
      names[normal]++;
      p += name.size();
      if (*p == ';') p++;
    }
  }
}

// What's drawn against published facts, independent of the generator
static void testKnownCrystals() {
  const uint8_t H = 1, C = 6, O = 8, F = 9, Na = 11, Si = 14, S = 16, Cl = 17, Ca = 20, Fe = 26, Cu = 29, Ag = 47, Au = 79;

  CHECK(build(get("salt")));                 // Rock salt: a cube of 3 x 3 x 3 atoms, Na and Cl taking turns
  CHECK(nAtoms == 27 && nBonds == 54);
  CHECK(countElement(Na) + countElement(Cl) == 27);
  CHECK(countElement(Na) == 13 || countElement(Na) == 14);
  NEAR(shortest(Na, Cl), 5.62f / 2, 0.01f);
  for (int a = 0; a < nAtoms; a++) {
    for (int k = adjStart[a]; k < adjStart[a + 1]; k++) CHECK(atoms[a].element != atoms[adj[k]].element);
  }

  CHECK(build(get("diamond")));              // 8 corners, 6 faces, 4 inside; each inner atom has 4 neighbours
  CHECK(nAtoms == 18 && nBonds == 16);
  NEAR(shortest(C, C), 1.545f, 0.005f);

  CHECK(build(get("silicon")));
  CHECK(nAtoms == 18 && nBonds == 16);
  NEAR(shortest(Si, Si), 2.352f, 0.005f);

  CHECK(build(get("graphite")));             // Flat layers of hexagons: no lines between layers, 3.35 A apart
  NEAR(shortest(C, C), 1.42f, 0.01f);
  float layerGap = 1e9f;
  for (int a = 0; a < nAtoms; a++) {
    for (int k = adjStart[a]; k < adjStart[a + 1]; k++) NEAR(atoms[a].z * perUnit, atoms[adj[k]].z * perUnit, 0.01f);
    for (int b = 0; b < nAtoms; b++) {
      const float dz = fabs(atoms[a].z - atoms[b].z) * perUnit;
      if (dz > 0.5f) layerGap = std::min(layerGap, dz);
    }
  }
  NEAR(layerGap, 3.355f, 0.005f);
  for (int a = 0; a < nAtoms; a++) CHECK(adjStart[a + 1] - adjStart[a] <= 3);

  CHECK(build(get("ice")));                  // Oxygen only: the hydrogens' sites are half filled in the record
  CHECK(countElement(H) == 0);
  CHECK(countElement(O) == nAtoms);
  NEAR(shortest(O, O), 2.755f, 0.01f);
  for (int a = 0; a < nAtoms; a++) CHECK(adjStart[a + 1] - adjStart[a] <= 4);  // At most 4 neighbours, as in real ice

  CHECK(build(get("quartz")));               // Si bonded to O, each Si at most 4
  NEAR(shortest(Si, O), 1.61f, 0.01f);
  for (int a = 0; a < nAtoms; a++) {
    if (atoms[a].element == Si) CHECK(adjStart[a + 1] - adjStart[a] <= 4);
  }

  CHECK(build(get("calcite")));
  NEAR(shortest(C, O), 1.286f, 0.005f);
  NEAR(shortest(Ca, O), 2.357f, 0.005f);
  for (int a = 0; a < nAtoms; a++) {
    if (atoms[a].element == C) CHECK(adjStart[a + 1] - adjStart[a] <= 3);  // Carbonate: 3 O around each C
  }

  CHECK(build(get("pyrite")));
  NEAR(shortest(Fe, S), 2.264f, 0.005f);
  NEAR(shortest(S, S), 2.156f, 0.005f);

  CHECK(build(get("fluorite")));
  NEAR(shortest(Ca, F), 2.365f, 0.005f);
  CHECK(countElement(F) == 8 && countElement(Ca) == 14);

  CHECK(build(get("copper")));               // Face-centred cube: 14 atoms, 12 neighbours in bulk
  CHECK(nAtoms == 14 && nBonds == 36);
  NEAR(shortest(Cu, Cu), 2.556f, 0.005f);
  CHECK(build(get("gold")));
  CHECK(nAtoms == 14 && nBonds == 36);
  NEAR(shortest(Au, Au), 2.884f, 0.005f);
  CHECK(build(get("silver")));
  NEAR(shortest(Ag, Ag), 2.889f, 0.005f);

  CHECK(build(get("iron")));                 // Body-centred: 8 neighbours, two cells across
  CHECK(nAtoms == 35 && nBonds == 64);
  NEAR(shortest(Fe, Fe), 2.477f, 0.005f);
}

static void testLimits() {
  const CrystalEntry& salt = get("salt");
  std::vector<CrystalAtom> few(26);
  std::vector<uint16_t> start(CRYSTAL_MAX_ATOMS + 1), links(107);
  uint16_t n, b;
  float u;
  CHECK(!crystalBuild(salt, few.data(), 26, start.data(), links.data(), 108, n, b, u));  // One atom short
  CHECK(n == 0);
  CHECK(!crystalBuild(salt, atoms.data(), CRYSTAL_MAX_ATOMS, start.data(), links.data(), 107, n, b, u));  // One link short
  CHECK(n == 0);
  CHECK(crystalBuild(salt, atoms.data(), 27, start.data(), adj.data(), 108, n, b, u));
  CHECK(!crystalBuild(salt, nullptr, 27, start.data(), adj.data(), 108, n, b, u));
  // A cell that can't exist
  CrystalEntry bad = salt;
  bad.gamma = 0;
  CHECK(!build(bad));
  bad = salt;
  bad.alpha = 170; bad.beta = 10; bad.gamma = 10;
  CHECK(!build(bad));
  CHECK(crystalAtomRadius(14, 29, 0) > crystalAtomRadius(100, 29, 0));
  CHECK(crystalAtomRadius(14, 6, 0) < crystalAtomRadius(14, 29, 0));
  CHECK(crystalAtomRadius(14, 29, 2) > crystalAtomRadius(14, 29, 0));
}

int main() {
  testResolve();
  testEveryEntry();
  testKnownCrystals();
  testLimits();
  printf("crystal: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
