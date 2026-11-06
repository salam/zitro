// Host tests for the Wolfram result parser, formula reader and layout. See run.sh.
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

#define PROGMEM
typedef struct {
  uint16_t bitmapOffset;
  uint8_t width, height, xAdvance;
  int8_t xOffset, yOffset;
} GFXglyph;
typedef struct {
  uint8_t* bitmap;
  GFXglyph* glyph;
  uint16_t first, last;
  uint8_t yAdvance;
} GFXfont;

#include "../../sketch/sketch_jan12a/fonts/PrimaryFont10pt.h"
#include "../../sketch/sketch_jan12a/fonts/PrimaryFont14pt.h"
#if __has_include(<lgfx/Fonts/Custom/DejaVu9.h>)
#include <lgfx/Fonts/Custom/DejaVu9.h>
#define WOLFRAM_TEST_HAS_DEJAVU 1
#endif
#include "../../sketch/sketch_jan12a/wolfram_layout.h"

static int failures = 0, checks = 0;

#define CHECK(cond)                                         \
  do {                                                      \
    checks++;                                               \
    if (!(cond)) {                                          \
      failures++;                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                       \
  } while (0)

static void checkEq(const std::string& got, const std::string& want, const char* what) {
  checks++;
  if (got != want) {
    failures++;
    printf("FAIL %s\n  got  %s\n  want %s\n", what, got.c_str(), want.c_str());
  }
}

// ---------------------------------------------------------------------------------------------------------
// Fonts: real glyph data. The small font comes with M5GFX; without that library here, 6 px per letter stands in.

static const GFXglyph* glyphFor(const GFXfont& font, uint32_t cp) {
  if (cp < font.first || cp > font.last) return nullptr;
  return &font.glyph[cp - font.first];
}

static int measureReal(uint8_t font, const char* text, int len) {
  int width = 0, i = 0;
  while (i < len) {
    uint32_t cp = 0;
    int n = 0;
    if (!wolframDecodeUtf8(text + i, len - i, cp, n)) {
      cp = '?';
      n = 1;
    }
    if (font == WOLFRAM_FONT_SMALL) {
#ifdef WOLFRAM_TEST_HAS_DEJAVU
      const GFXglyph* g = glyphFor(DejaVu9, cp);
      width += g ? g->xAdvance : 0;
#else
      width += 6;
#endif
    } else {
      const GFXfont& f = font == WOLFRAM_FONT_14 ? Roboto_Regular14pt8b : Roboto_Regular10pt8b;
      const GFXglyph* g = glyphFor(f, cp);
      width += g ? g->xAdvance : 0;
    }
    i += n;
  }
  return width;
}

static int measureWide(uint8_t, const char*, int len) {
  return len * 11;  // A nastier font: everything wraps
}

// ---------------------------------------------------------------------------------------------------------
// Helpers

static std::string readFile(const std::string& path) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) {
    printf("Can't read %s\n", path.c_str());
    exit(1);
  }
  std::string out;
  char buffer[4096];
  size_t n;
  while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) out.append(buffer, n);
  fclose(f);
  return out;
}

static std::string spanText(const WolframDoc& doc, WolframSpan s) {
  return std::string(doc.text + s.start, s.len);
}

static std::vector<std::string> fixtureNames(const std::string& dir) {
  std::vector<std::string> names;
  DIR* d = opendir(dir.c_str());
  if (!d) {
    printf("Can't open %s\n", dir.c_str());
    exit(1);
  }
  while (dirent* e = readdir(d)) {
    std::string n = e->d_name;
    if (n.size() > 4 && n.substr(n.size() - 4) == ".txt") names.push_back(n.substr(0, n.size() - 4));
  }
  closedir(d);
  std::sort(names.begin(), names.end());
  return names;
}

// What makes a number that number: its digits, point and signs. A lost minus has to show up here.
// "->" goes first: on screen a formula's arrow is drawn, and its "-" isn't a sign.
static std::string numbersOf(const std::string& s) {
  std::string plain;
  for (char c : s) {
    if (c != ' ') plain += c;
  }
  std::string out;
  for (size_t i = 0; i < plain.size(); i++) {
    const char c = plain[i];
    if (c == '-' && i + 1 < plain.size() && plain[i + 1] == '>') {
      i++;
    } else if ((c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-') {
      out += c;
    } else if (c == '\xC2' && i + 1 < plain.size() && plain[i + 1] == '\xB1') {  // Plus-minus
      out += "+-";
      i++;
    }
  }
  return out;
}

static WolframDoc* gDoc;
static WolframLayout* gLayout;
static WolframScratch* gScratch;

// A section's first page, if it has text
static const WolframPage* firstTextPage(const WolframLayout& L, int section) {
  for (int p = 0; p < L.pageCount; p++) {
    if (L.pages[p].section == section && !L.pages[p].plot) return &L.pages[p];
  }
  return nullptr;
}

// Text pieces of a section in the order they're laid out: every page, then its pinned notes once. No title.
static std::string shownText(const WolframDoc& doc, const WolframLayout& L, int section) {
  std::string out;
  for (int p = 0; p < L.pageCount; p++) {
    const WolframPage& page = L.pages[p];
    if (page.section != section || page.plot) continue;
    for (int i = 0; i < page.count; i++) {
      const WolframOp& op = L.ops[page.first + i];
      if (op.type == WOLFRAM_OP_TEXT) out += std::string(doc.text + op.a, op.b);
    }
  }
  const WolframPage* page = firstTextPage(L, section);
  if (page) {
    for (int i = 0; i < page->noteCount; i++) {
      const WolframOp& op = L.ops[page->noteFirst + i];
      if (op.type == WOLFRAM_OP_TEXT) out += std::string(doc.text + op.a, op.b);
    }
  }
  return out;
}

static std::string pageText(const WolframDoc& doc, const WolframLayout& L, const WolframPage& page) {
  std::string out;
  for (int i = 0; i < page.count; i++) {
    const WolframOp& op = L.ops[page.first + i];
    if (op.type == WOLFRAM_OP_TEXT) out += std::string(doc.text + op.a, op.b) + " ";
  }
  for (int i = 0; i < page.noteCount; i++) {
    const WolframOp& op = L.ops[page.noteFirst + i];
    if (op.type == WOLFRAM_OP_TEXT) out += std::string(doc.text + op.a, op.b) + " ";
  }
  return out;
}

static std::string shownNumbers(const WolframDoc& doc, const WolframLayout& L, int section) {
  return numbersOf(shownText(doc, L, section));
}

// The numbers Wolfram sent for a section, in the order the screen has them
static std::string sourceNumbers(const WolframDoc& doc, const WolframLayout& L, int section) {
  std::string flow, pinned;
  const WolframSection& s = doc.sections[section];
  const WolframPage* page = firstTextPage(L, section);
  const bool usesPins = page && page->noteCount > 0;
  for (int i = 0; i < s.lineCount; i++) {
    const WolframLine& line = doc.lines[s.firstLine + i];
    (usesPins && wolframLineIsPinned(doc.text, line) ? pinned : flow) += numbersOf(spanText(doc, line.span));
  }
  if (section == 0) {
    for (int i = 0; i < doc.assumptionCount; i++) {
      (usesPins ? pinned : flow) += numbersOf(spanText(doc, doc.assumptions[i]));
    }
  }
  return flow + pinned;
}

// Everything the screen promises: on it, no character a font lacks, every laid-out section's digits all there
static void checkLayoutInvariants(const WolframDoc& doc, const WolframLayout& L, WolframMeasure measure,
                                  const char* what) {
  for (int i = 0; i < L.opCount; i++) {
    const WolframOp& op = L.ops[i];
    if (op.type == WOLFRAM_OP_TEXT) {
      std::string text(doc.text + op.a, op.b);
      size_t at = 0;
      bool ok = true;
      while (at < text.size()) {
        uint32_t cp = 0;
        int n = 0;
        if (!wolframDecodeUtf8(text.data() + at, text.size() - at, cp, n)) {
          ok = false;
          break;
        }
        if (cp > 0x5FF) ok = false;
        if (op.font == WOLFRAM_FONT_SMALL && cp >= 0x80) ok = false;
        at += n;
      }
      if (!ok) {
        failures++;
        printf("FAIL %s: text piece a font can't draw: '%s'\n", what, text.c_str());
      }
      const int w = measure(op.font, text.data(), (int)text.size());
      if (op.x < 0 || op.x + w > WOLFRAM_SCREEN_WIDTH) {
        failures++;
        printf("FAIL %s: text off the screen at x=%d w=%d: '%s'\n", what, op.x, w, text.c_str());
      }
      checks++;
    } else {
      const int x1 = (int16_t)op.a;
      if (op.x < -1 || x1 < -1 || op.x > WOLFRAM_SCREEN_WIDTH || x1 > WOLFRAM_SCREEN_WIDTH) {
        failures++;
        printf("FAIL %s: line off the screen\n", what);
      }
      checks++;
    }
  }
  std::vector<bool> seen(doc.sectionCount, false);
  for (int p = 0; p < L.pageCount; p++) {
    if (!L.pages[p].plot) seen[L.pages[p].section] = true;
  }
  for (int s = 0; s < doc.sectionCount; s++) {
    if (!seen[s]) continue;
    const std::string shown = shownNumbers(doc, L, s), source = sourceNumbers(doc, L, s);
    checks++;
    if (shown != source) {
      failures++;
      printf("FAIL %s: section %d numbers changed\n  source %s\n  shown  %s\n", what, s, source.c_str(), shown.c_str());
    }
  }
}

static std::string dumpNode(const WolframFormula& f, int16_t i) {
  if (i < 0) return "";
  const WolframNode& n = f.nodes[i];
  const std::string raw(f.text + n.start, n.len);
  std::string out;
  switch (n.type) {
    case WOLFRAM_NODE_ROW:
      for (int16_t c = n.a; c >= 0; c = f.nodes[c].next) {
        if (!out.empty()) out += " ";
        out += dumpNode(f, c);
      }
      return out;
    case WOLFRAM_NODE_TEXT:
    case WOLFRAM_NODE_OP:
    case WOLFRAM_NODE_SYMBOL:
      return raw;
    case WOLFRAM_NODE_GLOSS:
      return "gloss[" + raw + "]";
    case WOLFRAM_NODE_GROUP:
      return ((n.flags & WOLFRAM_FLAG_HIDDEN) ? "{" : "(") + dumpNode(f, n.a) + ((n.flags & WOLFRAM_FLAG_HIDDEN) ? "}" : ")");
    case WOLFRAM_NODE_FRAC:
      return "frac[" + dumpNode(f, n.a) + " | " + dumpNode(f, n.b) + "]";
    case WOLFRAM_NODE_ROOT:
      return raw + "[" + dumpNode(f, n.a) + "]";
    case WOLFRAM_NODE_SCRIPT:
    case WOLFRAM_NODE_BIGOP:
      out = n.type == WOLFRAM_NODE_SCRIPT ? "pow[" + dumpNode(f, n.a) : "big[" + raw;
      if (n.b >= 0) out += " _ " + dumpNode(f, n.b);
      if (n.c >= 0) out += " ^ " + dumpNode(f, n.c);
      return out + "]";
  }
  return out;
}

// ---------------------------------------------------------------------------------------------------------
// Parser, on real answers

static bool parseFixture(const std::string& dir, const std::string& name, bool german) {
  const std::string body = readFile(dir + "/" + name + ".txt");
  return wolframParse(body.data(), body.size(), german, *gDoc);
}

static void testFirstSections(const std::string& dir) {
  static const char* const expected[][2] = {
      {"big", "Result"},
      {"circle", "Result"},
      {"convert", "Result"},
      {"cuberoot", "Exact result"},
      {"defint", "Definite integral"},
      {"derivative", "Derivative"},
      {"distance", "Current result"},
      {"fraction", "Exact result"},
      {"inequality", "Solutions"},
      {"integral", "Indefinite integral"},
      {"kinetic", "Equation"},
      {"limit", "Limit"},
      {"matrix", "Result"},
      {"mercury", "Result"},
      {"neq", "Solutions"},
      {"primes", "Result"},
      {"pythag", "Statement"},
      {"quad2", "Solutions"},
      {"quadratic", "Solutions"},
      {"series", "Infinite sum"},
      {"solve", "Result"},
      {"sqrt", "Decimal approximation"},
      {"sum", "Result"},
      {"torus", "Result"},
  };
  for (const auto& row : expected) {
    CHECK(parseFixture(dir, row[0], false));
    checkEq(spanText(*gDoc, gDoc->sections[0].title), row[1], row[0]);
  }
  // Wolfram said it couldn't: nothing to show, so the picture gets its turn
  CHECK(!parseFixture(dir, "gibberish", false));
  CHECK(!parseFixture(dir, "no_result_501", false));
}

static void testNoiseIsGone(const std::string& dir) {
  for (const std::string& name : fixtureNames(dir)) {
    if (!parseFixture(dir, name, false)) continue;
    for (int i = 0; i < gDoc->lineCount; i++) {
      const std::string line = spanText(*gDoc, gDoc->lines[i].span);
      CHECK(line.find("assumption=") == std::string::npos);
      CHECK(line.find("image:") != 0);
      CHECK(line.find("http") != 0);
      CHECK(line.find("Wolfram Language code") == std::string::npos);
      CHECK(line.empty() || line.back() != '|');
    }
    for (int i = 0; i < gDoc->sectionCount; i++) {
      const std::string title = spanText(*gDoc, gDoc->sections[i].title);
      CHECK(title != "Query" && title != "Assumption" && title != "Assumptions");
      CHECK(title.find("website result") == std::string::npos);
      CHECK(gDoc->sections[i].lineCount > 0 || gDoc->sections[i].isPlot);
    }
  }
}

static bool sectionHasLine(const WolframDoc& doc, int section, const std::string& text) {
  const WolframSection& s = doc.sections[section];
  for (int i = 0; i < s.lineCount; i++) {
    if (spanText(doc, doc.lines[s.firstLine + i].span) == text) return true;
  }
  return false;
}

static void testAssumptionsStayVisible(const std::string& dir) {
  // "mass of mercury" is the atomic mass only because Wolfram assumed so. That has to show with the number.
  CHECK(parseFixture(dir, "mercury", false));
  CHECK(sectionHasLine(*gDoc, 0, "200.592 u (unified atomic mass units)"));
  CHECK(gDoc->assumptionCount == 2);
  checkEq(spanText(*gDoc, gDoc->assumptions[0]), "Assuming \"mass\" is referring to elements", "mercury 1");
  checkEq(spanText(*gDoc, gDoc->assumptions[1]), "Assuming atomic mass", "mercury 2");
  checkEq(spanText(*gDoc, gDoc->label), "mercury | atomic mass", "mercury label");

  CHECK(parseFixture(dir, "torus", false));
  const WolframSection& first = gDoc->sections[0];
  CHECK(first.lineCount == 2);
  checkEq(spanText(*gDoc, gDoc->lines[first.firstLine + 1].span), "(assuming inner radius b cm)", "torus note");
  CHECK(gDoc->lines[first.firstLine + 1].note);
  CHECK(!gDoc->lines[first.firstLine].note);

  CHECK(parseFixture(dir, "cuberoot", false));
  CHECK(gDoc->assumptionCount == 1);
  checkEq(spanText(*gDoc, gDoc->assumptions[0]), "Assuming \"cube root\" is the real-valued root", "cube root");

  // "Calculate kinetic energy" and "For mass set ..." are instructions, not assumptions
  CHECK(parseFixture(dir, "kinetic", false));
  CHECK(gDoc->assumptionCount == 0);
  CHECK(sectionHasLine(*gDoc, 0, "K = (m v^2)/2"));
  CHECK(sectionHasLine(*gDoc, 0, "K | kinetic energy"));
  CHECK(sectionHasLine(*gDoc, 0, "(the energy of an object due to its motion)"));
}

static void testSectionOrder(const std::string& dir) {
  // Answer, then what was understood, then the rest as Wolfram had it
  CHECK(parseFixture(dir, "mercury", false));
  checkEq(spanText(*gDoc, gDoc->sections[1].title), "Input interpretation", "order 1");
  checkEq(spanText(*gDoc, gDoc->sections[2].title), "Unit conversions", "order 2");

  // Nested headers with no blank line, and empty ones
  CHECK(parseFixture(dir, "derivative", false));
  bool sawDomain = false, sawProperties = false;
  for (int i = 0; i < gDoc->sectionCount; i++) {
    const std::string t = spanText(*gDoc, gDoc->sections[i].title);
    if (t == "Domain") sawDomain = true;
    if (t == "Properties as a real function") sawProperties = true;
  }
  CHECK(sawDomain);
  CHECK(!sawProperties);
  // Only a remark about a picture that isn't here ("drawn with inner radius 1 cm"): no page
  CHECK(parseFixture(dir, "torus", false));
  for (int i = 0; i < gDoc->sectionCount; i++) {
    CHECK(spanText(*gDoc, gDoc->sections[i].title) != "Visual representation");
  }
}

static void testPlots(const std::string& dir) {
  CHECK(parseFixture(dir, "derivative", false));
  checkEq(spanText(*gDoc, gDoc->plotUrl), "https://public5c.wolframalpha.com/files/PNG_tjusfnr1jl.png", "first plot");
  int plots = 0;
  for (int i = 0; i < gDoc->sectionCount; i++) {
    if (gDoc->sections[i].isPlot) plots++;
  }
  CHECK(plots == 1);
  CHECK(parseFixture(dir, "limit", false));
  CHECK(gDoc->plotUrl.len > 0);
  CHECK(parseFixture(dir, "defint", false));
  CHECK(gDoc->plotUrl.len > 0);
  // A number line is no plot worth a download, and an image without code isn't either
  CHECK(parseFixture(dir, "sum", false));
  CHECK(gDoc->plotUrl.len == 0);
  CHECK(parseFixture(dir, "fraction", false));
  CHECK(gDoc->plotUrl.len == 0);
  CHECK(parseFixture(dir, "matrix", false));
  CHECK(gDoc->plotUrl.len == 0);

  static const char* const good[] = {"https://public5c.wolframalpha.com/files/PNG_a1.png",
                                     "https://public6.wolframalpha.com/files/PNG_tk-1.png"};
  for (const char* url : good) CHECK(wolframPlotUrlOk(url, strlen(url)));
  static const char* const bad[] = {"http://public5c.wolframalpha.com/files/a.png",
                                    "https://wolframalpha.com.evil.example/a.png",
                                    "https://evil.example/.wolframalpha.com/a.png",
                                    "https://user@public5c.wolframalpha.com/a.png",
                                    "https://.wolframalpha.com/a.png",
                                    "https://public5c.wolframalpha.com/a.png?x=1",
                                    "https://public5c.wolframalpha.com/a.gif",
                                    "https://public5c.wolframalpha.com/../a b.png",
                                    "https://public5c.wolframalpha.com",
                                    ""};
  for (const char* url : bad) CHECK(!wolframPlotUrlOk(url, strlen(url)));
  // A plot URL from elsewhere is ignored and the answer still stands
  const std::string hostile = "Result:\n10\n\nPlot:\nimage: https://evil.example/x.png\nWolfram Language code: Plot[x, {x, 0, 1}]\n";
  CHECK(wolframParse(hostile.data(), hostile.size(), false, *gDoc));
  CHECK(gDoc->plotUrl.len == 0);
  CHECK(gDoc->sectionCount == 1);
}

static void testGerman(const std::string& dir) {
  CHECK(parseFixture(dir, "torus", true));
  checkEq(spanText(*gDoc, gDoc->sections[0].title), "Ergebnis", "title");
  checkEq(spanText(*gDoc, gDoc->sections[1].title), "Verstanden als", "title 2");
  // Wolfram's own words and numbers stay as they came
  checkEq(spanText(*gDoc, gDoc->lines[gDoc->sections[0].firstLine + 1].span), "(assuming inner radius b cm)", "note");
  CHECK(parseFixture(dir, "quad2", true));
  checkEq(spanText(*gDoc, gDoc->sections[0].title), "L\xC3\xB6sungen", "umlaut");
  // A title it doesn't know stays English
  CHECK(parseFixture(dir, "primes", true));
  bool english = false;
  for (int i = 0; i < gDoc->sectionCount; i++) {
    if (spanText(*gDoc, gDoc->sections[i].title) == "Teiler") english = true;
  }
  CHECK(english);
  CHECK(parseFixture(dir, "pythag", true));
  bool kept = false;
  for (int i = 0; i < gDoc->sectionCount; i++) {
    if (spanText(*gDoc, gDoc->sections[i].title) == "Alternate name") kept = true;
  }
  CHECK(kept);
}

// ---------------------------------------------------------------------------------------------------------
// Formula reader: hand-written expected shapes for real lines

static std::string readFormula(const std::string& line, bool& ok) {
  static std::string text;
  text = line;
  WolframSpan span = {0, (uint16_t)text.size()};
  ok = wolframFormulaParse(text.c_str(), span, gScratch->formula);
  if (!ok) return "";
  checks++;
  if (!wolframFormulaRoundTrips(gScratch->formula, span)) {
    failures++;
    printf("FAIL round trip: %s\n", line.c_str());
  }
  return dumpNode(gScratch->formula, gScratch->formula.root);
}

static void testFormulaShapes() {
  static const char* const cases[][2] = {
      // Basics
      {"10", "10"},
      {"x = ±2", "x = ± 2"},
      {"7/12", "frac[7 | 12]"},
      // Wolfram reads left to right: after the bar, a space starts a new factor
      {"1/2 e^(-i x) x^2", "frac[1 | 2] pow[e ^ {- i x}] pow[x ^ 2]"},
      {"1/4 (b - 5)^2 (b + 5) π^2", "frac[1 | 4] pow[(b - 5) ^ 2] (b + 5) pow[π ^ 2]"},
      // The bar replaces the parentheses
      {"K = (m v^2)/2", "K = frac[{m pow[v ^ 2]} | 2]"},
      {"1/(2 n)", "frac[1 | {2 n}]"},
      {"(sin^2(x))/(2 π)", "frac[{pow[sin ^ 2] (x)} | {2 π}]"},
      // Each fraction on its own
      {"x = -1/2 - sqrt(5)/2", "x = - frac[1 | 2] - frac[sqrt[{5}] | 2]"},
      {"x = sqrt(5)/2 - 1/2", "x = frac[sqrt[{5}] | 2] - frac[1 | 2]"},
      {"(x + 1/2)^2 - 5/4 = 0", "pow[(x + frac[1 | 2]) ^ 2] - frac[5 | 4] = 0"},
      {"4/5 (x + 1/2)^2 = 1", "frac[4 | 5] pow[(x + frac[1 | 2]) ^ 2] = 1"},
      // Calculus
      {"d/dx(x^2 sin(x)) = x (2 sin(x) + x cos(x))",
       "frac[d | dx] (pow[x ^ 2] sin (x)) = x (2 sin (x) + x cos (x))"},
      {"integral sin(x) dx = -cos(x) + constant", "big[integral] sin (x) dx = - cos (x) + constant"},
      {"integral_0^1 x^2 dx = 1/3≈0.33333", "big[integral _ 0 ^ 1] pow[x ^ 2] dx = frac[1 | 3] ≈ 0.33333"},
      {"integral_0^(2 π) (sin^2(x))/(2 π) dx = 1/2 = 0.5",
       "big[integral _ 0 ^ {2 π}] frac[{pow[sin ^ 2] (x)} | {2 π}] dx = frac[1 | 2] = 0.5"},
      {"integral_0^π sin(x) dx = 2", "big[integral _ 0 ^ π] sin (x) dx = 2"},
      {"lim_(x->0) sin(x)/x = 1", "big[lim _ {x -> 0}] frac[sin (x) | x] = 1"},
      {"sum_(n=1)^∞ 1/n^2 = π^2/6≈1.6449",
       "big[sum _ {n = 1} ^ ∞] frac[1 | pow[n ^ 2]] = frac[pow[π ^ 2] | 6] ≈ 1.6449"},
      {"π^2/6 = -2 sum_(k=1)^∞ (-1)^k/k^2",
       "frac[pow[π ^ 2] | 6] = - 2 big[sum _ {k = 1} ^ ∞] frac[pow[(- 1) ^ k] | pow[k ^ 2]]"},
      {"π^2/6 = 8/3 ( sum_(k=0)^∞ (-1)^k/(1 + 2 k))^2",
       "frac[pow[π ^ 2] | 6] = frac[8 | 3] pow[(big[sum _ {k = 0} ^ ∞] frac[pow[(- 1) ^ k] | {1 + 2 k}]) ^ 2]"},
      {"sum_(n=1)^k 1/n^2 = H_k^(2)", "big[sum _ {n = 1} ^ k] frac[1 | pow[n ^ 2]] = pow[H _ k ^ (2)]"},
      // The order of a derivative isn't a power: its parentheses stay
      {"f^(n)(x)", "pow[f ^ (n)] (x)"},
      {"x^(n + 1)", "pow[x ^ {n + 1}]"},
      {"1 - x^2/6 + x^4/120 + O(x^6)", "1 - frac[pow[x ^ 2] | 6] + frac[pow[x ^ 4] | 120] + O (pow[x ^ 6])"},
      {"-1/2 e^(-i x) - e^(i x)/2 + constant", "- frac[1 | 2] pow[e ^ {- i x}] - frac[pow[e ^ {i x}] | 2] + constant"},
      // Powers of ten and roots
      {"1.267650600228229401496703205376 × 10^30", "1.267650600228229401496703205376 × pow[10 ^ 30]"},
      {"3.33091 × 10^-22 grams", "3.33091 × pow[10 ^ - 22] grams"},
      {"5^(1/3)", "pow[5 ^ {frac[1 | 3]}]"},
      {"5^(1/3) e^((2 i π)/3) ≈ -0.8550 + 1.4809 i",
       "pow[5 ^ {frac[1 | 3]}] pow[e ^ {frac[{2 i π} | 3]}] ≈ - 0.8550 + 1.4809 i"},
      {"5^(1/3) e^(-(2 i π)/3) ≈ -0.8550 -1.4809 i",
       "pow[5 ^ {frac[1 | 3]}] pow[e ^ {- frac[{2 i π} | 3]}] ≈ - 0.8550 - 1.4809 i"},
      {"cuberoot(5)", "cuberoot[{5}]"},
      {"sqrt(2) e^(i π) ≈ -1.4142 (real root)", "sqrt[{2}] pow[e ^ {i π}] ≈ - 1.4142 gloss[(real root)]"},
      {"sqrt(2) e^0 ≈ 1.41421 (real, principal root)", "sqrt[{2}] pow[e ^ 0] ≈ 1.41421 gloss[(real, principal root)]"},
      // Relations written with no spaces
      {"x>2", "x > 2"},
      {"x<-2", "x < - 2"},
      {"2<x<=5", "2 < x <= 5"},
      {"(x - 2) (x + 2)>0", "(x - 2) (x + 2) > 0"},
      // Units and what's said about them
      {"9 π cm^2 (square centimeters)≈28.2743 cm^2 (square centimeters)",
       "9 π pow[cm ^ 2] gloss[(square centimeters)] ≈ 28.2743 pow[cm ^ 2] gloss[(square centimeters)]"},
      {"1/4 (b - 5)^2 (b + 5) π^2 cm^3 (cubic centimeters)≈2.4674 (b - 5)^2 (b + 5) cm^3 (cubic centimeters)",
       "frac[1 | 4] pow[(b - 5) ^ 2] (b + 5) pow[π ^ 2] pow[cm ^ 3] gloss[(cubic centimeters)] ≈ 2.4674 pow[(b - 5) ^ 2] "
       "(b + 5) pow[cm ^ 3] gloss[(cubic centimeters)]"},
      {"10 m/s (meters per second)", "10 frac[m | s] gloss[(meters per second)]"},
      {"200.592 u (unified atomic mass units)", "200.592 u gloss[(unified atomic mass units)]"},
      {"2^3 × 3^2 × 5 (6 prime factors, 3 distinct)", "pow[2 ^ 3] × pow[3 ^ 2] × 5 gloss[(6 prime factors, 3 distinct)]"},
      {"6.214 miles", "6.214 miles"},
      {"-2", "- 2"},
      {"5!", "5 !"},
      // Numbers Wolfram says go on
      {"1.41421356...", "1.41421356..."},
      {"x ≈ ±11.1727058683300...", "x ≈ ± 11.1727058683300..."},
  };
  for (const auto& row : cases) {
    bool ok = false;
    const std::string got = readFormula(row[0], ok);
    if (!ok) {
      failures++;
      checks++;
      printf("FAIL not read as a formula: %s\n", row[0]);
      continue;
    }
    checkEq(got, row[1], row[0]);
  }

  // Never shown as if right: these are plain text
  static const char* const notFormulas[] = {
      "0.583^_ (period 1)",
      "[1; 2^_]",
      "(2, 5]",
      "(2, ∞)",
      "(-∞, -2)",
      "7/12 | 14/24 | 21/36",
      "mass | 1 kg (kilogram)",
      "{x!=2, x<=5}",
      "1/2x",           // x above or below the bar? Wolfram writes x/2 or 1/(2 x)
      "x^2y",           // same
      "1/x(x + 1)",     // same
      "a/b/c",          // read differently in different places
      "12/25/2026",     // a date
      "x^y^z",
      "x ^2",
      "^^^",
      "x^",
      "x^_",
      "x_",
      "/2",
      "2/",
      "x + ",
      "= =",
      "x = = 2",
      "()",
      "(",
      ")",
      "(x",
      "x)",
      "and/or",
      "miles/hour",
      "The Pythagorean theorem states that for a right triangle with legs of length a and b, a^2 + b^2 = c^2.",
      "surjective onto real numbers",
      " ≈ 0.92 × greatest ocean depth ( 10924 m )",
      "1.2.3",
      "c^2.",
      "x; y",
      "a, b",
      "€5",
      "x∈Z",
      "שלום",
      "",
  };
  for (const char* line : notFormulas) {
    bool ok = true;
    const std::string got = readFormula(line, ok);
    checks++;
    if (ok) {
      failures++;
      printf("FAIL read as a formula, should be text: '%s' as %s\n", line, got.c_str());
    }
  }

  // "product" and "sum" and "integral" as plain words aren't signs
  bool ok = false;
  checkEq(readFormula("dot product", ok), "dot product", "word product");
  checkEq(readFormula("integral", ok), "integral", "word integral");
  ok = true;
  readFormula("integral of x", ok);
  CHECK(ok);
  checkEq(readFormula("integral of x", ok), "integral of x", "no dx, no sign");
}

// ---------------------------------------------------------------------------------------------------------
// Layout, on real answers

static void testLayoutOnFixtures(const std::string& dir) {
  for (const std::string& name : fixtureNames(dir)) {
    for (int german = 0; german < 2; german++) {
      if (!parseFixture(dir, name, german)) continue;
      for (int plot = 0; plot < 2; plot++) {
        CHECK(wolframLayoutBuild(*gDoc, plot, measureReal, *gLayout, *gScratch));
        checkLayoutInvariants(*gDoc, *gLayout, measureReal, name.c_str());
      }
      CHECK(wolframLayoutBuild(*gDoc, false, measureWide, *gLayout, *gScratch));
      checkLayoutInvariants(*gDoc, *gLayout, measureWide, (name + " wide").c_str());
      // Every section has its pages: nothing in a real answer gets dropped for lack of room
      wolframLayoutBuild(*gDoc, true, measureReal, *gLayout, *gScratch);
      std::vector<bool> seen(gDoc->sectionCount, false);
      for (int p = 0; p < gLayout->pageCount; p++) seen[gLayout->pages[p].section] = true;
      for (int s = 0; s < gDoc->sectionCount; s++) CHECK(seen[s]);
    }
  }
}

static void testAssumptionsOnScreen(const std::string& dir) {
  // The number and what it depends on: on the same screen, on every page of that section
  CHECK(parseFixture(dir, "mercury", true));
  CHECK(wolframLayoutBuild(*gDoc, false, measureReal, *gLayout, *gScratch));
  std::string first = pageText(*gDoc, *gLayout, gLayout->pages[0]);
  CHECK(first.find("200.592") != std::string::npos);
  CHECK(first.find("atomic") != std::string::npos);
  CHECK(first.find("Assuming") != std::string::npos);

  CHECK(parseFixture(dir, "torus", true));
  CHECK(wolframLayoutBuild(*gDoc, false, measureReal, *gLayout, *gScratch));
  first = pageText(*gDoc, *gLayout, gLayout->pages[0]);
  CHECK(first.find("assuming") != std::string::npos);
  CHECK(first.find("inner") != std::string::npos);
  for (int p = 0; p < gLayout->pageCount && gLayout->pages[p].section == 0; p++) {
    CHECK(pageText(*gDoc, *gLayout, gLayout->pages[p]).find("assuming") != std::string::npos);
  }

  CHECK(parseFixture(dir, "cuberoot", true));
  CHECK(wolframLayoutBuild(*gDoc, false, measureReal, *gLayout, *gScratch));
  CHECK(pageText(*gDoc, *gLayout, gLayout->pages[0]).find("real-valued") != std::string::npos);

  // So many assumptions that they'd crowd out the answer: they go on in the normal flow, nothing lost
  std::string crowded = "Assumptions:\n";
  for (int i = 0; i < 4; i++) crowded += "Assuming something long enough to wrap over several rows of this small screen\n";
  crowded += "\nResult:\n42\n";
  CHECK(wolframParse(crowded.data(), crowded.size(), false, *gDoc));
  CHECK(wolframLayoutBuild(*gDoc, false, measureReal, *gLayout, *gScratch));
  checkLayoutInvariants(*gDoc, *gLayout, measureReal, "crowded");
  CHECK(pageText(*gDoc, *gLayout, gLayout->pages[0]).find("42") != std::string::npos);
}

static void testNumbersUnchanged(const std::string& dir) {
  // Numbers exactly as Wolfram sent them, whole, in the laid-out text. Pinned by hand, not derived.
  static const char* const pinned[][2] = {
      {"big", "1267650600228229401496703205376"},
      {"big", "1.267650600228229401496703205376"},
      {"sqrt", "1.4142135623730950488016887242096980785696718753769480731766797379..."},
      {"fraction", "0.5833333333333333333333333333333333333333333333333333333333333333..."},
      {"cuberoot", "1.7099759466766969893531088725438601098680551105430549243828617074..."},
      {"cuberoot", "1.70998"},
      {"torus", "2.4674"},
      {"circle", "28.2743"},
      {"mercury", "200.592"},
      {"mercury", "3.33091"},
      {"convert", "6.214"},
      {"convert", "1.1"},
      {"derivative", "11.1727058683300..."},
      {"derivative", "2.28892972810340..."},
      {"defint", "0.33333"},
      {"series", "1.6449"},
      {"distance", "389052"},
  };
  for (const auto& row : pinned) {
    CHECK(parseFixture(dir, row[0], true));
    CHECK(wolframLayoutBuild(*gDoc, false, measureReal, *gLayout, *gScratch));
    std::string all;
    for (int s = 0; s < gDoc->sectionCount; s++) all += shownText(*gDoc, *gLayout, s);
    checks++;
    if (all.find(row[1]) == std::string::npos) {
      failures++;
      printf("FAIL %s: number not shown whole: %s\n", row[0], row[1]);
    }
    CHECK(wolframLayoutBuild(*gDoc, false, measureWide, *gLayout, *gScratch));
    all.clear();
    for (int s = 0; s < gDoc->sectionCount; s++) all += shownText(*gDoc, *gLayout, s);
    checks++;
    if (all.find(row[1]) == std::string::npos) {
      failures++;
      printf("FAIL %s, wide font: number not shown whole: %s\n", row[0], row[1]);
    }
  }
}

static void testSymbolsAreDrawn(const std::string& dir) {
  CHECK(parseFixture(dir, "series", false));
  CHECK(wolframLayoutBuild(*gDoc, false, measureReal, *gLayout, *gScratch));
  std::string all;
  for (int s = 0; s < gDoc->sectionCount; s++) all += shownText(*gDoc, *gLayout, s);
  CHECK(all.find("\xE2\x89\x88") == std::string::npos);  // approx sign
  CHECK(all.find("\xE2\x88\x9E") == std::string::npos);  // infinity
  CHECK(all.find("sum") == std::string::npos);           // Drawn as a sign
  int lines = 0;
  for (int i = 0; i < gLayout->opCount; i++) {
    if (gLayout->ops[i].type == WOLFRAM_OP_LINE) lines++;
  }
  CHECK(lines > 20);

  // The same symbols in text that isn't a formula. A character nobody can draw gets a box, never a gap.
  const std::string text = "Result:\nx \xE2\x89\x88 3 \xE2\x89\xA4 \xE2\x88\x9E \xE2\x89\xA0 \xE2\x88\xAB, \xE2\x88\x91, \xE2\x88\x9A!\n\nOther:\n1, \xE2\x88\x88 2\n";
  CHECK(wolframParse(text.data(), text.size(), false, *gDoc));
  CHECK(wolframLayoutBuild(*gDoc, false, measureReal, *gLayout, *gScratch));
  checkLayoutInvariants(*gDoc, *gLayout, measureReal, "symbols in text");

  // On the first page, a character nobody can draw means the picture is the better answer
  const std::string first = "Result:\nx \xE2\x88\x88 Z\n";
  CHECK(!wolframParse(first.data(), first.size(), false, *gDoc));
  const std::string later = "Result:\n5\n\nOther:\nx \xE2\x88\x88 Z\n";
  CHECK(wolframParse(later.data(), later.size(), false, *gDoc));
  CHECK(wolframLayoutBuild(*gDoc, false, measureReal, *gLayout, *gScratch));
  checkLayoutInvariants(*gDoc, *gLayout, measureReal, "box");
}

// ---------------------------------------------------------------------------------------------------------
// Broken and hostile input

static void parseAndLayout(const std::string& body, const char* what) {
  if (!wolframParse(body.data(), body.size(), true, *gDoc)) return;
  CHECK(gDoc->sectionCount > 0);
  if (wolframLayoutBuild(*gDoc, true, measureReal, *gLayout, *gScratch)) {
    checkLayoutInvariants(*gDoc, *gLayout, measureReal, what);
  }
}

static void testHostile(const std::string& dir) {
  // Not an answer at all
  CHECK(!wolframParse("", 0, false, *gDoc));
  CHECK(!wolframParse(nullptr, 0, false, *gDoc));
  static const char* const nothing[] = {
      "\n\n\n",
      "Query:\n",
      "Query:\n\"5 + 5\"\n",
      "Result:\n",
      "Result:\n\nInput:\n\nPlot:\n",
      "Plot:\nimage: https://public5c.wolframalpha.com/files/a.png\nWolfram Language code: Plot[x, {x, 0, 1}]\n",
      "Wolfram|Alpha could not understand: x. \nYou could instead try: y",
      "<html><body>Bad gateway</body></html>",
      "Assumptions:\nAssuming nothing\n",
      ":\n:\n:\n",
  };
  for (const char* body : nothing) {
    if (wolframParse(body, strlen(body), false, *gDoc)) {
      failures++;
      printf("FAIL parsed nothing into something: %s\n", body);
    }
    checks++;
  }

  // Bytes that can't be text
  const std::string good = "Result:\n10\n";
  CHECK(wolframParse(good.data(), good.size(), false, *gDoc));
  const std::string withNul = std::string("Result:\n1\0" "0\n", 11);
  CHECK(!wolframParse(withNul.data(), withNul.size(), false, *gDoc));
  const std::string cutChar = "Result:\nx \xE2\x89";  // Cut in the middle of the approx sign
  CHECK(!wolframParse(cutChar.data(), cutChar.size(), false, *gDoc));
  const std::string overlong = "Result:\n\xC0\xAF\n";
  CHECK(!wolframParse(overlong.data(), overlong.size(), false, *gDoc));
  const std::string surrogate = "Result:\n\xED\xA0\x80\n";
  CHECK(!wolframParse(surrogate.data(), surrogate.size(), false, *gDoc));
  const std::string control = "Result:\n\x1B[31m10\n";
  CHECK(!wolframParse(control.data(), control.size(), false, *gDoc));
  const std::string tooLong(WOLFRAM_BODY_CAPACITY + 1, 'x');
  CHECK(!wolframParse(tooLong.data(), tooLong.size(), false, *gDoc));

  // Line ends from elsewhere
  const std::string crlf = "Query:\r\n\"5\"\r\n\r\nResult:\r\n10 \r\n\r\nNumber name:\r\nten\r\n";
  CHECK(wolframParse(crlf.data(), crlf.size(), false, *gDoc));
  checkEq(spanText(*gDoc, gDoc->sections[0].title), "Result", "CRLF title");
  CHECK(sectionHasLine(*gDoc, 0, "10"));
  CHECK(gDoc->sectionCount == 2);

  // Long lines: wrapped or dropped whole, never overrun
  parseAndLayout("Result:\n" + std::string(6000, '9') + "\n", "long number");
  parseAndLayout("Result:\n" + std::string(6000, 'x') + "\n", "long word");
  parseAndLayout("Result:\n" + std::string(6000, ' ') + "5\n", "spaces");
  parseAndLayout("Result:\n" + std::string(3000, '(') + std::string(3000, ')') + "\n", "deep parens");
  parseAndLayout("Result:\n" + std::string(500, '(') + "x" + std::string(500, ')') + "\n", "500 deep");
  parseAndLayout("Result:\n" + std::string(200, '(') + "x\n", "unbalanced");
  parseAndLayout("Result:\n" + std::string(200, '^') + "\n", "carets");
  parseAndLayout("Result:\n" + std::string(200, '/') + "\n", "slashes");
  parseAndLayout("Result:\n" + std::string(200, '_') + "\n", "underscores");
  std::string nested = "x";
  for (int i = 0; i < 40; i++) nested = "(" + nested + ")^(" + nested.substr(0, 3) + ")/2";
  parseAndLayout("Result:\n" + nested.substr(0, 5000) + "\n", "nested");
  std::string fractions = "Result:\n";
  for (int i = 0; i < 200; i++) fractions += "1/2 ";
  parseAndLayout(fractions + "\n", "many fractions");
  std::string roots = "Result:\n";
  for (int i = 0; i < 30; i++) roots += "sqrt(";
  parseAndLayout(roots + "x\n", "open roots");

  // More of everything than there's room for: sections and lines drop whole, never half
  std::string many;
  for (int i = 0; i < 400; i++) many += "Section " + std::to_string(i) + ":\n" + std::to_string(i) + "\n\n";
  parseAndLayout(many.substr(0, 6000), "many sections");
  std::string lines = "Result:\n";
  for (int i = 0; i < 1200; i++) lines += std::to_string(i % 10) + "\n";
  CHECK(lines.size() < WOLFRAM_BODY_CAPACITY);
  CHECK(!wolframParse(lines.data(), lines.size(), false, *gDoc));  // The only section would be half a list
  std::string lines2 = "Result:\n5\n\nSolutions:\n";
  for (int i = 0; i < 1200; i++) lines2 += "x\n";
  CHECK(wolframParse(lines2.data(), lines2.size(), false, *gDoc));
  CHECK(gDoc->sectionCount == 1);
  CHECK(gDoc->lineCount == 1);

  // Assumption lines beyond the limit, instructions disguised as lines
  parseAndLayout("Assumptions:\nAssuming a\nAssuming b\nAssuming c\nAssuming d\nAssuming e\nAssuming f\n\nResult:\n5\n", "assumptions");
  const std::string instructions = "Assumptions:\nAssuming x set assumption=*C.x-_*Y-\n\nResult:\n5\nset assumption=*C.x\n";
  CHECK(wolframParse(instructions.data(), instructions.size(), false, *gDoc));
  CHECK(gDoc->assumptionCount == 0);
  CHECK(gDoc->lineCount == 1);

  // Random damage to real answers. Whatever comes out must keep every promise; the sanitizers watch memory.
  uint32_t seed = 0x2F6E2B1;
  const auto next = [&]() {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
  };
  const std::vector<std::string> names = fixtureNames(dir);
  static const char interesting[] = "()^_/=|:\n \xE2\x89\x88x1.-+";
  for (int round = 0; round < 3000; round++) {
    std::string body = readFile(dir + "/" + names[next() % names.size()] + ".txt");
    const int edits = 1 + next() % 8;
    for (int e = 0; e < edits && !body.empty(); e++) {
      const size_t at = next() % body.size();
      switch (next() % 4) {
        case 0:
          body[at] = interesting[next() % (sizeof(interesting) - 1)];
          break;
        case 1:
          body.erase(at, 1 + next() % 40);
          break;
        case 2:
          body.insert(at, 1 + next() % 12, interesting[next() % (sizeof(interesting) - 1)]);
          break;
        default:
          body.resize(at);
          break;
      }
    }
    parseAndLayout(body, "fuzz");
  }
  // And lines of pure operators and atoms for the formula reader alone
  static const char* const pieces[] = {"(", ")", "^", "_", "/", "x", "2", " ", "sqrt(", "sum_", "integral", "dx",
                                       "=", "-", "+", "≈", "∞", "π", "!", "...", ".", "sin", "(cubic meters)", "->"};
  for (int round = 0; round < 20000; round++) {
    std::string line;
    const int count = 1 + next() % 24;
    for (int i = 0; i < count; i++) line += pieces[next() % (sizeof(pieces) / sizeof(pieces[0]))];
    bool ok = false;
    readFormula(line, ok);  // Round trip is checked inside for all that parse
    if (ok) {
      const std::string body = "Result:\n" + line + "\n";
      parseAndLayout(body, "fuzz formula");
    }
  }
}

static void testPlotHelpers() {
  static const uint8_t png[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R',
                                0, 0, 3, 0x87, 0, 0, 1, 0xBE};
  uint32_t w = 0, h = 0;
  CHECK(wolframPngSize(png, sizeof(png), w, h));
  CHECK(w == 903 && h == 446);
  CHECK(!wolframPngSize(png, 23, w, h));
  CHECK(!wolframPngSize(nullptr, 0, w, h));
  uint8_t gif[24];
  memcpy(gif, "GIF89a", 6);
  memset(gif + 6, 0, 18);
  CHECK(!wolframPngSize(gif, sizeof(gif), w, h));
  uint8_t empty[24];
  memcpy(empty, png, 24);
  memset(empty + 16, 0, 8);
  CHECK(!wolframPngSize(empty, 24, w, h));

  int fw = 0, fh = 0;
  wolframFitSize(903, 446, 240, 117, fw, fh);
  CHECK(fw == 236 && fh == 117);
  wolframFitSize(100, 50, 240, 117, fw, fh);  // Never larger
  CHECK(fw == 100 && fh == 50);
  wolframFitSize(4000, 2, 240, 117, fw, fh);
  CHECK(fw == 240 && fh == 1);
  wolframFitSize(2, 4000, 240, 117, fw, fh);
  CHECK(fw == 1 && fh == 117);
  wolframFitSize(4294967295u, 4294967295u, 240, 117, fw, fh);
  CHECK(fw == 117 && fh == 117);

  // A thin dark line on white shrinks to a grey line, not to nothing
  const int sw = 40, sh = 40;
  std::vector<uint16_t> src(sw * sh, 0xFFFF);
  for (int y = 0; y < sh; y++) src[y * sw + 21] = 0x0000;
  std::vector<uint16_t> dst(10 * 10, 0);
  wolframShrink565(src.data(), sw, sh, dst.data(), 10, 10);
  CHECK(dst[0] == 0xFFFF);
  CHECK(dst[5] != 0xFFFF && dst[5] != 0x0000);
  // Swapped bytes come back swapped: pure red stays pure red
  std::vector<uint16_t> red(16, 0x00F8);  // 0xF800 with its bytes swapped
  std::vector<uint16_t> one(1, 0);
  wolframShrink565(red.data(), 4, 4, one.data(), 1, 1);
  CHECK(one[0] == 0x00F8);
}

// ---------------------------------------------------------------------------------------------------------
// A picture of every page, to look at. Draws what the device would draw, with the real Roboto glyphs.

struct Picture {
  int width, height;
  std::vector<uint8_t> rgb;
  void set(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    uint8_t* p = &rgb[(size_t)(y * width + x) * 3];
    p[0] = r;
    p[1] = g;
    p[2] = b;
  }
};

static void colorOf(uint8_t color, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (color == WOLFRAM_COLOR_TITLE) {
    r = 0xF2; g = 0xB7; b = 0x05;
  } else if (color == WOLFRAM_COLOR_DIM) {
    r = g = b = 0xC0;
  } else {
    r = g = b = 0xFF;
  }
}

static void drawGlyph(Picture& pic, const GFXfont& f, uint32_t cp, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  const GFXglyph* gl = glyphFor(f, cp);
  if (!gl) return;
  const uint8_t* bits = f.bitmap + gl->bitmapOffset;
  int bit = 0;
  for (int yy = 0; yy < gl->height; yy++) {
    for (int xx = 0; xx < gl->width; xx++, bit++) {
      if (bits[bit / 8] & (0x80 >> (bit % 8))) pic.set(x + gl->xOffset + xx, y + gl->yOffset + yy, r, g, b);
    }
  }
}

static void drawOp(Picture& pic, const WolframDoc& doc, const WolframOp& op, int ox, int oy) {
  uint8_t r, g, b;
  colorOf(op.color, r, g, b);
  if (op.type == WOLFRAM_OP_LINE) {
    int x0 = op.x, y0 = op.y;
    const int x1 = (int16_t)op.a, y1 = (int16_t)op.b;
    const int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
      pic.set(ox + x0, oy + y0, r, g, b);
      if (x0 == x1 && y0 == y1) break;
      const int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
    return;
  }
  int x = op.x, i = 0;
  while (i < op.b) {
    uint32_t cp = 0;
    int n = 0;
    if (!wolframDecodeUtf8(doc.text + op.a + i, op.b - i, cp, n)) break;
#ifdef WOLFRAM_TEST_HAS_DEJAVU
    const GFXfont& small = DejaVu9;
#else
    const GFXfont& small = Roboto_Regular10pt8b;
#endif
    const GFXfont& f = op.font == WOLFRAM_FONT_14 ? Roboto_Regular14pt8b
                                                  : (op.font == WOLFRAM_FONT_10 ? Roboto_Regular10pt8b : small);
    drawGlyph(pic, f, cp, ox + x, oy + op.y, r, g, b);
    x += measureReal(op.font, doc.text + op.a + i, n);
    i += n;
  }
}

static void writeBmp(const Picture& pic, const std::string& path) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return;
  const int stride = (pic.width * 3 + 3) & ~3;
  const uint32_t size = 54 + stride * pic.height;
  uint8_t header[54] = {'B', 'M'};
  const auto put32 = [&](int at, uint32_t v) {
    for (int i = 0; i < 4; i++) header[at + i] = (uint8_t)(v >> (8 * i));
  };
  put32(2, size);
  put32(10, 54);
  put32(14, 40);
  put32(18, (uint32_t)pic.width);
  put32(22, (uint32_t)pic.height);
  header[26] = 1;
  header[28] = 24;
  fwrite(header, 1, 54, f);
  std::vector<uint8_t> row(stride, 0);
  for (int y = pic.height - 1; y >= 0; y--) {
    for (int x = 0; x < pic.width; x++) {
      const uint8_t* p = &pic.rgb[(size_t)(y * pic.width + x) * 3];
      row[x * 3] = p[2];
      row[x * 3 + 1] = p[1];
      row[x * 3 + 2] = p[0];
    }
    fwrite(row.data(), 1, stride, f);
  }
  fclose(f);
}

static void writePreview(const std::string& dir, const std::string& path) {
  std::vector<std::pair<std::string, int>> pages;
  for (const std::string& name : fixtureNames(dir)) {
    if (!parseFixture(dir, name, true)) continue;
    wolframLayoutBuild(*gDoc, true, measureReal, *gLayout, *gScratch);
    for (int p = 0; p < gLayout->pageCount; p++) pages.push_back({name, p});
  }
  const int columns = 4, cellW = 244, cellH = 139;
  const int rows = ((int)pages.size() + columns - 1) / columns;
  Picture pic = {columns * cellW, rows * cellH, {}};
  pic.rgb.assign((size_t)pic.width * pic.height * 3, 0x30);
  int index = 0;
  for (const auto& entry : pages) {
    const int ox = (index % columns) * cellW + 2, oy = (index / columns) * cellH + 2;
    for (int y = 0; y < 135; y++) {
      for (int x = 0; x < 240; x++) pic.set(ox + x, oy + y, 0, 0, 0);
    }
    parseFixture(dir, entry.first, true);
    wolframLayoutBuild(*gDoc, true, measureReal, *gLayout, *gScratch);
    const WolframPage& page = gLayout->pages[entry.second];
    if (page.plot) {
      for (int y = 0; y < 117; y++) {
        for (int x = 0; x < 236; x++) pic.set(ox + 2 + x, oy + y, 0xE0, 0xE0, 0xE0);
      }
    } else {
      for (int i = 0; i < page.titleCount; i++) drawOp(pic, *gDoc, gLayout->ops[page.titleFirst + i], ox, oy);
      for (int i = 0; i < page.noteCount; i++) drawOp(pic, *gDoc, gLayout->ops[page.noteFirst + i], ox, oy);
      for (int i = 0; i < page.count; i++) drawOp(pic, *gDoc, gLayout->ops[page.first + i], ox, oy);
    }
    for (int x = 0; x < 240; x++) pic.set(ox + x, oy + 117, 0x50, 0x50, 0x50);
    index++;
  }
  writeBmp(pic, path);
  printf("Wrote %d pages to %s\n", (int)pages.size(), path.c_str());
}

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : "fixtures";
  gDoc = new WolframDoc;
  gLayout = new WolframLayout;
  gScratch = new WolframScratch;

  // Real Wolfram answers stay local: its terms don't allow passing them on (see docs/features/wolfram.md)
  DIR* fixtureDir = opendir(dir.c_str());
  const bool haveFixtures = fixtureDir != nullptr;
  if (fixtureDir) closedir(fixtureDir);
  if (!haveFixtures) printf("No Wolfram fixtures here: skipping the checks that need real answers\n");
  testFormulaShapes();
  testPlotHelpers();
  if (haveFixtures) {
    testFirstSections(dir);
    testNoiseIsGone(dir);
    testAssumptionsStayVisible(dir);
    testSectionOrder(dir);
    testPlots(dir);
    testGerman(dir);
    testLayoutOnFixtures(dir);
    testAssumptionsOnScreen(dir);
    testNumbersUnchanged(dir);
    testSymbolsAreDrawn(dir);
    testHostile(dir);
    if (argc > 2) writePreview(dir, argv[2]);
  }

  printf("%d checks, %d failed\n", checks, failures);
  delete gDoc;
  delete gLayout;
  delete gScratch;
  return failures == 0 ? 0 : 1;
}
