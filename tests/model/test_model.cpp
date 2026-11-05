// Host tests for the 3D view's pure logic. Build and run: tests/model/run.sh
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../../sketch/sketch_jan12a/model_core.h"

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
  CHECK(r.kind == MODEL_MOLECULE);
  CHECK(strcmp(r.query, "caffeine") == 0);
  CHECK(parseModelRef("molecule:sodium lauryl sulfate", r));
  const char* bad[] = {"", "molecule:", "sphere:x", ":x", "caffeine", "molecule:a\nb"};
  for (const char* s : bad) {
    r.kind = MODEL_MOLECULE;
    CHECK(!parseModelRef(s, r));
    CHECK(r.kind == MODEL_NONE);
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

int main(int argc, char** argv) {
  fixtureDir = argc > 1 ? argv[1] : "fixtures";
  testRefs(); testSdf(); testMath(); testElements();
  printf("%d checks, %d failures\n", checks, failures);
  if (failures == 0) printf("All model tests passed\n");
  return failures == 0 ? 0 : 1;
}
