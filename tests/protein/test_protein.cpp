// Host tests for the protein view's pure logic. Build and run: tests/protein/run.sh
// The fixtures are the real generated files in webpage2/data/protein.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../sketch/sketch_jan12a/protein_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, e) CHECK(fabs((a) - (b)) <= (e))

static std::string dataDir;
static std::string readFile(const std::string& name) {
  std::string path = dataDir + "/" + name, out;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) { fprintf(stderr, "missing %s\n", path.c_str()); exit(2); }
  char buf[4096]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
  fclose(f);
  return out;
}

static std::vector<ProteinPoint> pts(PROTEIN_MAX_POINTS);
static uint16_t nPoints, nChains;
static bool parseAs(const std::string& s, const char* id) {
  return proteinParseStructure(s.data(), s.size(), id, pts.data(), PROTEIN_MAX_POINTS, nPoints, nChains);
}
static bool resolve(const std::string& index, const char* q, ProteinEntry& e) {
  return proteinResolve(index.data(), index.size(), q, e);
}

static void testNormalise() {
  char o[PROTEIN_NORMALISED_SIZE];
  CHECK(proteinNormalise("Hemoglobin", o, sizeof o) && !strcmp(o, "hemoglobin"));
  CHECK(proteinNormalise("  DNA  ", o, sizeof o) && !strcmp(o, "dna"));
  CHECK(proteinNormalise("HIV-1  protease", o, sizeof o) && !strcmp(o, "hiv 1 protease"));
  CHECK(proteinNormalise("H\xC3\xA4moglobin", o, sizeof o) && !strcmp(o, "haemoglobin"));
  CHECK(proteinNormalise("\xC3\x9C" "b\xC3\xB6r\xC3\x9F", o, sizeof o) && !strcmp(o, "ueboerss"));
  const char* bad[] = {"", "   ", "---", "a|b", "a;b", "a\nb", "\xCE\xB1-amylase", "\xC3", "\xC3\xA9", "a\x7F"};
  for (const char* s : bad) CHECK(!proteinNormalise(s, o, sizeof o));
  CHECK(!proteinNormalise(nullptr, o, sizeof o));
  char tiny[4];
  CHECK(!proteinNormalise("abcd", tiny, sizeof tiny));
  CHECK(proteinNormalise("abc", tiny, sizeof tiny));
  CHECK(!proteinNormalise("\xC3\xA4\xC3\xA4", tiny, sizeof tiny));
}

static void testIndex() {
  const std::string index = readFile("index.txt");
  CHECK(index.size() < PROTEIN_INDEX_LIMIT);
  ProteinEntry e;
  CHECK(resolve(index, "hemoglobin", e));
  CHECK(!strcmp(e.id, "4HHB"));
  CHECK(!strcmp(e.de, "Haemoglobin"));
  CHECK(!strcmp(e.en, "Hemoglobin"));
  const struct { const char* q; const char* id; } known[] = {
      {"Hemoglobin", "4HHB"}, {"H\xC3\xA4moglobin", "4HHB"}, {"haemoglobin", "4HHB"}, {"lysozyme", "1LYZ"},
      {"Lysozym", "1LYZ"}, {"insulin", "4INS"}, {"GFP", "1EMA"}, {"green fluorescent protein", "1EMA"},
      {"catalase", "7CAT"}, {"DNA", "1BNA"}, {"dna double helix", "1BNA"}, {"amylase", "1SMD"},
      {"HIV-1 protease", "1HVR"}, {"Antik\xC3\xB6rper", "1IGT"}, {"trypsin", "2PTN"}, {"pepsin", "4PEP"}};
  for (const auto& k : known) {
    CHECK(resolve(index, k.q, e));
    CHECK(!strcmp(e.id, k.id));
  }
  // The AI can't pick a record: an ID is not a name. Nor do near misses or general words count.
  const char* unknown[] = {"4HHB", "4hhb", "1LYZ", "1lyz", "168L", "1BNA", "enzyme", "protein", "Enzym", "hemo",
                           "hemoglobi", "hemoglobinn", "human hemoglobin", "nonsense", "hemoglobin;lysozyme",
                           "hemoglobin|1LYZ", "../../etc/passwd", "4HHB|Hemoglobin", "END", "ZITRO-PROTEIN-INDEX 1 50"};
  for (const char* q : unknown) {
    CHECK(!resolve(index, q, e));
    CHECK(e.id[0] == 0);
  }
  CHECK(!resolve(index, "", e));
  CHECK(!proteinResolve(nullptr, 0, "dna", e));
  CHECK(!proteinResolve(index.data(), index.size(), nullptr, e));
}

static const std::string TINY_INDEX =
    "ZITRO-PROTEIN-INDEX 1 2\n4HHB|Haemoglobin|Hemoglobin|hemoglobin;haemoglobin\n1BNA|DNA|DNA|dna\nEND\n";

static void testHostileIndex() {
  ProteinEntry e;
  CHECK(resolve(TINY_INDEX, "dna", e));
  CHECK(resolve("ZITRO-PROTEIN-INDEX 1 1\r\n1BNA|DNA|DNA|dna\r\nEND\r\n", "dna", e));  // CRLF is harmless
  // Every cut-off version is refused, even if the wanted name is already in it
  for (size_t cut = 0; cut + 1 < TINY_INDEX.size(); cut++) {
    CHECK(!proteinResolve(TINY_INDEX.data(), cut, "hemoglobin", e));
    CHECK(e.id[0] == 0);
  }
  const char* broken[] = {
      "",
      "\n",
      "1BNA|DNA|DNA|dna\nEND\n",                                  // No header
      "ZITRO-PROTEIN-INDEX 2 1\n1BNA|DNA|DNA|dna\nEND\n",         // Unknown version
      "ZITRO-PROTEIN-INDEX 1 2\n1BNA|DNA|DNA|dna\nEND\n",         // Wrong count
      "ZITRO-PROTEIN-INDEX 1 0\n1BNA|DNA|DNA|dna\nEND\n",
      "ZITRO-PROTEIN-INDEX 1 x\n1BNA|DNA|DNA|dna\nEND\n",
      "ZITRO-PROTEIN-INDEX  1 1\n1BNA|DNA|DNA|dna\nEND\n",
      "ZITRO-PROTEIN-INDEX 1 1\n1BNA|DNA|DNA|dna\n",              // No end
      "ZITRO-PROTEIN-INDEX 1 1\n1BNA|DNA|DNA|dna\nEND\nextra\n",  // Something after the end
      "ZITRO-PROTEIN-INDEX 1 1\n../x|DNA|DNA|dna\nEND\n",         // Not an ID: can't become a path
      "ZITRO-PROTEIN-INDEX 1 1\n1bna|DNA|DNA|dna\nEND\n",
      "ZITRO-PROTEIN-INDEX 1 1\n1BN/|DNA|DNA|dna\nEND\n",
      "ZITRO-PROTEIN-INDEX 1 1\n1BNAA|DNA|DNA|dna\nEND\n",
      "ZITRO-PROTEIN-INDEX 1 1\n1BNA||DNA|dna\nEND\n",            // Empty label
      "ZITRO-PROTEIN-INDEX 1 1\n1BNA|DNA||dna\nEND\n",
      "ZITRO-PROTEIN-INDEX 1 1\n1BNA|D\xC3\x84|DNA|dna\nEND\n",   // Non-ASCII label
      "ZITRO-PROTEIN-INDEX 1 1\n1BNA|DNA|DNA|\nEND\n",            // No name
      "ZITRO-PROTEIN-INDEX 1 1\n1BNA|DNA|DNA|dna;\nEND\n",
      "ZITRO-PROTEIN-INDEX 1 1\n1BNA|DNA|DNA\nEND\n",             // Field missing
      "ZITRO-PROTEIN-INDEX 1 1\n1BNA|DNA|DNA|dna|x\nEND\n",       // Field too many
      "ZITRO-PROTEIN-INDEX 1 1\n1BNA|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA|DNA|dna\nEND\n",  // Too long
      "ZITRO-PROTEIN-INDEX 1 2\n1BNA|DNA|DNA|dna\n4HHB|Hb|Hb|dna\nEND\n",  // One name, two records
  };
  for (const char* s : broken) {
    CHECK(!resolve(s, "dna", e));
    CHECK(e.id[0] == 0);
  }
  // NUL bytes and damage anywhere never crash or read outside the buffer
  for (size_t i = 0; i < TINY_INDEX.size(); i++) {
    for (char c : {'\0', '|', ';', '\n', ' ', (char)0xFF}) {
      std::string s = TINY_INDEX;
      s[i] = c;
      std::vector<char> exact(s.begin(), s.end());
      proteinResolve(exact.data(), exact.size(), "hemoglobin", e);
    }
  }
}

static const std::string TINY_FILE =
    "ZITRO-PROTEIN 1 1BNA 5 2\nC 3\n0 0 0\nG\n10 0 0\n20 0 0\nC 2\n0 10 0\n0 20 0\nEND\n";

static void testStructureBasics() {
  CHECK(parseAs(TINY_FILE, "1BNA"));
  CHECK(nPoints == 5 && nChains == 2);
  // Chain 1: starts blue (t 0), ends red (t 255), and the gap stops the line
  CHECK(pts[0].t == 0 && pts[1].t == 127 && pts[2].t == 255);
  CHECK(pts[0].connected == 0 && pts[1].connected == 0 && pts[2].connected == 1);
  // A new chain never connects to the one before
  CHECK(pts[3].t == 0 && pts[3].connected == 0 && pts[4].t == 255 && pts[4].connected == 1);
  float maxR = 0, sx = 0, sy = 0, sz = 0;
  for (int i = 0; i < 5; i++) {
    maxR = std::max(maxR, sqrtf(pts[i].x * pts[i].x + pts[i].y * pts[i].y + pts[i].z * pts[i].z));
    sx += pts[i].x; sy += pts[i].y; sz += pts[i].z;
  }
  NEAR(maxR, 1.0f, 1e-4f);
  NEAR(sx, 0, 1e-4f); NEAR(sy, 0, 1e-4f); NEAR(sz, 0, 1e-4f);
  CHECK(parseAs("ZITRO-PROTEIN 1 1BNA 2 2\nC 1\n0 0 0\nC 1\n5 5 5\nEND\n", "1BNA"));
  CHECK(pts[0].t == 0 && pts[1].t == 0);
  CHECK(parseAs("ZITRO-PROTEIN 1 1BNA 2 1\r\nC 2\r\n0 0 0\r\n5 5 5\r\nEND\r\n", "1BNA"));
}

static void testHostileStructure() {
  // The wrong record is refused: its label and ID on screen would lie
  CHECK(!parseAs(TINY_FILE, "4HHB"));
  CHECK(!parseAs(TINY_FILE, "1bna"));
  for (size_t cut = 0; cut + 1 < TINY_FILE.size(); cut++) {
    CHECK(!proteinParseStructure(TINY_FILE.data(), cut, "1BNA", pts.data(), PROTEIN_MAX_POINTS, nPoints, nChains));
    CHECK(nPoints == 0);
  }
  const char* broken[] = {
      "", "\n", "END\n", "hello world",
      "<html><body>404 Not Found</body></html>",
      "ZITRO-PROTEIN 2 1BNA 2 1\nC 2\n0 0 0\n5 5 5\nEND\n",     // Unknown version
      "ZITRO-PROTEIN 1 1BNA 2\nC 2\n0 0 0\n5 5 5\nEND\n",       // Field missing
      "ZITRO-PROTEIN 1 1BNA 3 1\nC 2\n0 0 0\n5 5 5\nEND\n",     // Too few points
      "ZITRO-PROTEIN 1 1BNA 1 1\nC 1\n0 0 0\nEND\n",            // One point isn't a structure
      "ZITRO-PROTEIN 1 1BNA 0 0\nEND\n",
      "ZITRO-PROTEIN 1 1BNA -2 1\nC 2\n0 0 0\n5 5 5\nEND\n",
      "ZITRO-PROTEIN 1 1BNA 2 2\nC 2\n0 0 0\n5 5 5\nEND\n",     // Too few chains
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 1\n0 0 0\n5 5 5\nEND\n",     // Point beyond the chain's size
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 3\n0 0 0\n5 5 5\nEND\n",     // Chain larger than promised
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 0\n0 0 0\n5 5 5\nEND\n",
      "ZITRO-PROTEIN 1 1BNA 2 1\nC -2\n0 0 0\n5 5 5\nEND\n",
      "ZITRO-PROTEIN 1 1BNA 2 1\n0 0 0\nC 2\n5 5 5\nEND\n",     // Point before any chain
      "ZITRO-PROTEIN 1 1BNA 2 1\nG\nC 2\n0 0 0\n5 5 5\nEND\n",  // Gap before any point
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\nG\n0 0 0\n5 5 5\nEND\n",  // Gap at the start of a chain
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\n5 5 5\nG\nEND\n",  // Gap at the end
      "ZITRO-PROTEIN 1 1BNA 3 1\nC 3\n0 0 0\nG\nG\n5 5 5\n6 6 6\nEND\n",
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\n5 5 5\n",          // No end
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\n5 5 5\nEND\nX\n",  // Something after the end
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\n5 5\nEND\n",       // Coordinate missing
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\n5 5 5 5\nEND\n",   // Coordinate too many
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\nnan inf 1\nEND\n",
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\n1e9 0 0\nEND\n",
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\n1.5 0 0\nEND\n",
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\n99999 0 0\nEND\n",
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\n999999999999 0 0\nEND\n",
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\n- 0 0\nEND\n",
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n0 0 0\n1  0 0\nEND\n",
      "ZITRO-PROTEIN 1 1BNA 2 1\nC 2\n7 7 7\n7 7 7\nEND\n",     // All in one spot
      "ZITRO-PROTEIN 1 1BNA 1501 1\nC 1501\n0 0 0\nEND\n",      // Over the limit
      "ZITRO-PROTEIN 1 1BNA 65537 1\nC 2\n0 0 0\n1 1 1\nEND\n", // Would wrap a 16-bit count
      "ZITRO-PROTEIN 1 1BNA 2 65\nC 2\n0 0 0\n1 1 1\nEND\n",
  };
  for (const char* s : broken) {
    CHECK(!parseAs(s, "1BNA"));
    CHECK(nPoints == 0);
  }
  // Fewer points than the file promises never overrun the caller's buffer
  std::vector<ProteinPoint> small(3);
  CHECK(!proteinParseStructure(TINY_FILE.data(), TINY_FILE.size(), "1BNA", small.data(), 3, nPoints, nChains));
  CHECK(!proteinParseStructure(nullptr, 5, "1BNA", small.data(), 3, nPoints, nChains));
  // Over the limit, line by line
  std::string big = "ZITRO-PROTEIN 1 1BNA 1500 1\nC 1500\n";
  for (int i = 0; i < 1500; i++) big += std::to_string(i) + " 0 0\n";
  CHECK(!parseAs(big, "1BNA"));
  big += "END\n";
  CHECK(parseAs(big, "1BNA"));
  CHECK(nPoints == 1500);
  std::string tooMany = big;
  tooMany.erase(tooMany.size() - 4);
  tooMany += "1500 0 0\nEND\n";
  CHECK(!parseAs(tooMany, "1BNA"));
  // Damage anywhere never crashes or reads outside the buffer. Run with the address sanitizer.
  const std::string real = readFile("1bna.txt");
  for (size_t i = 0; i < real.size(); i++) {
    for (char c : {'\0', '\n', ' ', 'G', 'C', '-', '9', (char)0xFF}) {
      std::string s = real;
      s[i] = c;
      std::vector<char> exact(s.begin(), s.end());
      proteinParseStructure(exact.data(), exact.size(), "1BNA", pts.data(), PROTEIN_MAX_POINTS, nPoints, nChains);
      if (nPoints) {
        for (uint16_t k = 0; k < nPoints; k++) CHECK(std::isfinite(pts[k].x) && std::isfinite(pts[k].y) && std::isfinite(pts[k].z));
      }
    }
  }
}

// Every record in the real index resolves and parses, and what matters about it is right
static void testRealData() {
  const std::string index = readFile("index.txt");
  CHECK(index.size() < PROTEIN_INDEX_LIMIT);
  char normal[PROTEIN_NORMALISED_SIZE];
  int records = 0;
  size_t pos = index.find('\n') + 1;
  while (pos < index.size()) {
    const size_t nl = index.find('\n', pos);
    const std::string line = index.substr(pos, nl - pos);
    pos = nl + 1;
    if (line == "END") break;
    records++;
    const std::string id = line.substr(0, 4);
    const size_t namesAt = line.rfind('|') + 1;
    std::string lower = id;
    for (char& c : lower) c = (char)tolower(c);
    size_t from = namesAt;
    while (from <= line.size()) {
      size_t to = line.find(';', from);
      if (to == std::string::npos) to = line.size();
      const std::string name = line.substr(from, to - from);
      ProteinEntry e;
      CHECK(resolve(index, name.c_str(), e));   // Every listed name finds this record, and no other
      CHECK(id == e.id);
      CHECK(proteinNormalise(name.c_str(), normal, sizeof normal));
      CHECK(name == normal);                    // Same rules as the script
      CHECK(name != lower);
      from = to + 1;
    }
    const std::string file = readFile(lower + ".txt");
    CHECK(file.size() < PROTEIN_FILE_LIMIT);
    CHECK(parseAs(file, id.c_str()));
    CHECK(nPoints >= 22 && nPoints <= PROTEIN_MAX_POINTS);
    ProteinEntry e;
    CHECK(!resolve(index, id.c_str(), e));      // Its own ID is not a name
    CHECK(!resolve(index, lower.c_str(), e));
    // Backbone steps stay physical, apart from the marked gaps: no line crosses a missing stretch
    uint16_t lines = 0;
    for (uint16_t i = 0; i < nPoints; i++) lines += pts[i].connected;
    CHECK(lines + nChains <= nPoints);
  }
  CHECK(records >= 30 && records <= 50);

  CHECK(parseAs(readFile("4hhb.txt"), "4HHB"));
  CHECK(nPoints == 574 && nChains == 4);
  CHECK(parseAs(readFile("1lyz.txt"), "1LYZ"));
  CHECK(nPoints == 129 && nChains == 1);
  CHECK(parseAs(readFile("4ins.txt"), "4INS"));
  CHECK(nPoints == 102 && nChains == 4);
  CHECK(parseAs(readFile("1bna.txt"), "1BNA"));
  CHECK(nPoints == 22 && nChains == 2);  // 12 base pairs; the first base of each strand has no P
  CHECK(parseAs(readFile("1ehz.txt"), "1EHZ"));
  CHECK(nPoints == 76 && nChains == 1);  // All 76 of tRNA, also the modified bases
}

static void testColours() {
  // Blue to red, the same way for any chain
  CHECK(proteinRampColor(0) == proteinRgb565(40, 90, 255));
  CHECK(proteinRampColor(255) == proteinRgb565(250, 40, 40));
  for (int t = 0; t < 256; t++) proteinRampColor((uint8_t)t);
  const uint16_t c = proteinRgb565(255, 255, 255);
  CHECK(proteinShade565(c, 3) == c);
  CHECK(proteinShade565(c, 0) < c);
  CHECK(proteinShade565(0, 2) == 0);
}

static void testDepthOrder() {
  const float z[6] = {0.9f, -0.9f, 0.0f, 1.0f, -1.0f, 0.5f};
  uint16_t order[6];
  proteinDepthOrder(z, 6, order);
  std::vector<uint16_t> seen(order, order + 6);
  std::sort(seen.begin(), seen.end());
  for (uint16_t i = 0; i < 6; i++) CHECK(seen[i] == i);  // Everyone is drawn, once
  for (int i = 1; i < 6; i++) CHECK(z[order[i - 1]] <= z[order[i]] + 2.0f / PROTEIN_DEPTH_BUCKETS);
  CHECK(order[0] == 4 && order[5] == 3);
  // Out-of-range and NaN stay inside the buckets
  const float odd[4] = {NAN, 5.0f, -5.0f, 0.0f};
  uint16_t o2[4];
  proteinDepthOrder(odd, 4, o2);
  std::vector<uint16_t> s2(o2, o2 + 4);
  std::sort(s2.begin(), s2.end());
  for (uint16_t i = 0; i < 4; i++) CHECK(s2[i] == i);
  // The largest model, worst order
  std::vector<float> zs(PROTEIN_MAX_POINTS);
  std::vector<uint16_t> os(PROTEIN_MAX_POINTS);
  for (int i = 0; i < PROTEIN_MAX_POINTS; i++) zs[i] = 1.0f - 2.0f * i / (PROTEIN_MAX_POINTS - 1);
  proteinDepthOrder(zs.data(), PROTEIN_MAX_POINTS, os.data());
  for (int i = 1; i < PROTEIN_MAX_POINTS; i++) CHECK(zs[os[i - 1]] <= zs[os[i]] + 2.0f / PROTEIN_DEPTH_BUCKETS);
  CHECK(proteinLineWidth(22, 0) > proteinLineWidth(1500, 0));
  CHECK(proteinLineWidth(1500, 0) >= 1);
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: test_protein <data dir>\n"); return 2; }
  dataDir = argv[1];
  testNormalise();
  testIndex();
  testHostileIndex();
  testStructureBasics();
  testHostileStructure();
  testRealData();
  testColours();
  testDepthOrder();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
