// Host tests for dino_core.h. The fixtures are inline; the generated files in webpage2/data/dino are
// checked as well when that folder exists.
#include <dirent.h>

#include <set>
#include <string>
#include <vector>

#include "../../sketch/sketch_jan12a/dino_core.h"

static int failures = 0;
static int checks = 0;
#define CHECK(c)                                                 \
  do {                                                           \
    checks++;                                                    \
    if (!(c)) {                                                  \
      failures++;                                                \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);        \
    }                                                            \
  } while (0)
#define CHECK_STR(a, b)                                          \
  do {                                                           \
    checks++;                                                    \
    if (strcmp((a), (b)) != 0) {                                 \
      failures++;                                                \
      printf("FAIL %s:%d: '%s' != '%s'\n", __FILE__, __LINE__, (a), (b)); \
    }                                                            \
  } while (0)

static std::string readFile(const std::string& path) {
  std::string out;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return out;
  char buffer[4096];
  size_t n;
  while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) out.append(buffer, n);
  fclose(f);
  return out;
}

// The font the stick uses for text, read directly so these widths are real ones
#define PROGMEM
struct GFXglyph {
  uint16_t bitmapOffset;
  uint8_t width, height, xAdvance;
  int8_t xOffset, yOffset;
};
struct GFXfont {
  uint8_t* bitmap;
  GFXglyph* glyph;
  uint16_t first, last;
  uint8_t yAdvance;
};
#include "../../sketch/sketch_jan12a/fonts/PrimaryFont10pt.h"

// As textWidth does it for this font: advances added up, per Unicode character
static int textWidth(const char* text) {
  const GFXfont& font = Roboto_Regular10pt8b;
  int width = 0;
  for (const unsigned char* p = (const unsigned char*)text; *p;) {
    uint32_t codepoint = *p++;
    if (codepoint >= 0xC0 && *p) codepoint = ((codepoint & 0x1F) << 6) | (*p++ & 0x3F);
    if (codepoint >= font.first && codepoint <= font.last) width += font.glyph[codepoint - font.first].xAdvance;
  }
  return width;
}

// Greedy wrap at spaces; a word wider than a line is cut where it overflows. Returns the line count.
static int wrapLines(const char* text, int maxWidth) {
  std::string line;
  int lines = 1;
  std::string word;
  std::string all = std::string(text) + " ";
  for (char c : all) {
    if (c != ' ') {
      word += c;
      continue;
    }
    if (word.empty()) continue;
    std::string trial = line.empty() ? word : line + " " + word;
    if (textWidth(trial.c_str()) <= maxWidth) {
      line = trial;
    } else {
      lines++;
      line = word;
      while (textWidth(line.c_str()) > maxWidth && line.size() > 1) {
        lines++;
        line = line.substr(line.size() / 2);
      }
    }
    word.clear();
  }
  return lines;
}

static const char* TREX_TEXT =
    "ZDS1\ttyrannosaurus\tQ14332\n"
    "name\tTyrannosaurus\n"
    "group\t0\n"
    "image\t9000\t240\t117\t0badf00d\tJohn Doe, CC BY 2.0, resized\n"
    "length\t1200\t1300\n"
    "height\t360\t400\n"
    "mass\t8800000\t8800000\n"
    "speed\t200\t300\n"
    "lived\t68000000\t66000000\n"
    "named\t1905\n"
    "era\tKreide\tCretaceous\n"
    "meaning\tK\xC3\xB6nig der Tyrannenechsen\tking of the tyrant lizards\n"
    "fun\tSein Biss war st\xC3\xA4rker als der jedes Landtiers.\tIts bite was stronger than any land animal's.\tWikipedia (EN): Tyrannosaurus\n"
    "source\tWikidata, Wikipedia\n"
    "end\n";

static bool parseText(const std::string& text, const char* slug, DinoSheet& s, std::vector<char>& storage) {
  storage.assign(text.begin(), text.end());
  storage.resize(text.size() + 1);
  return dinoParseSheet(storage.data(), text.size(), storage.size(), slug, s);
}

static bool parses(const std::string& text) {
  DinoSheet s;
  std::vector<char> storage;
  return parseText(text, "tyrannosaurus", s, storage);
}

static std::string trexWith(const std::string& from, const std::string& to) {
  std::string t = TREX_TEXT;
  size_t at = t.find(from);
  if (at == std::string::npos) {
    printf("FAIL fixture has no '%s'\n", from.c_str());
    failures++;
    return t;
  }
  return t.replace(at, from.size(), to);
}

static bool resolves(const char* indexText, const char* prefix, const char* normalised, const char* expected) {
  std::vector<char> index(indexText, indexText + strlen(indexText) + 1);
  char slug[48];
  if (!dinoResolve(index.data(), strlen(indexText), prefix, normalised, slug, sizeof(slug))) return false;
  return expected && strcmp(slug, expected) == 0;
}

static void testIndexName() {
  char name[64], file[32];
  CHECK(dinoIndexName("T-Rex", name, sizeof(name), file, sizeof(file)));
  CHECK_STR(name, "t rex");
  CHECK_STR(file, "index-t_.txt");
  CHECK(dinoIndexName("Tyrannosaurus", name, sizeof(name), file, sizeof(file)));
  CHECK_STR(file, "index-ty.txt");
  CHECK(dinoIndexName("\xC3\x84rgerosaurus", name, sizeof(name), file, sizeof(file)));
  CHECK_STR(name, "aergerosaurus");
  CHECK_STR(file, "index-ae.txt");
  CHECK(dinoIndexName("T", name, sizeof(name), file, sizeof(file)));
  CHECK_STR(file, "index-t_.txt");
  CHECK(dinoIndexName("T. rex", name, sizeof(name), file, sizeof(file)));
  CHECK_STR(file, "index-t_.txt");
  CHECK(!dinoIndexName("", name, sizeof(name), file, sizeof(file)));
  CHECK(!dinoIndexName("-", name, sizeof(name), file, sizeof(file)));
  CHECK(!dinoIndexName("Tyrannosaurus", name, sizeof(name), file, 8));  // No room for the file name
}

static void testResolveForms() {
  const char* ty = "ZDX1\tty\t2\ntyrannosaurus\ttyrannosaurus\ntyrannotitan\ttyrannotitan\n";
  CHECK(resolves(ty, "ty", "tyrannosaurus", "tyrannosaurus"));
  CHECK(resolves(ty, "ty", "tyrannosaurus rex", "tyrannosaurus"));
  CHECK(!resolves(ty, "ty", "tyranno", nullptr));
  CHECK(!resolves(ty, "ty", "tyrannosaurusx", nullptr));
  CHECK(!resolves(ty, "ty", "tyrannosaurus", "tyrannotitan"));
  CHECK(!resolves(ty, "xx", "tyrannosaurus", nullptr));  // Not this index's prefix

  const char* t_ = "ZDX1\tt_\t1\nt rex\ttyrannosaurus\n";
  CHECK(resolves(t_, "t_", "t rex", "tyrannosaurus"));    // "T-Rex"
  CHECK(resolves(t_, "t_", "t. rex", "tyrannosaurus"));   // "T. rex"
  CHECK(!resolves(t_, "t_", "t", nullptr));
  CHECK(!resolves(t_, "t_", "t rexy", nullptr));

  const char* ve = "ZDX1\tve\t2\nvelociraptor\tvelociraptor\nvelociraptor mongoliensis\tvelociraptor_m\n";
  CHECK(resolves(ve, "ve", "velociraptor mongoliensis", "velociraptor_m"));  // An exact entry wins
  CHECK(resolves(ve, "ve", "velociraptor osmolskae", "velociraptor"));      // A species falls back to its genus
  const char* ve1 = "ZDX1\tve\t1\nvelociraptor\tvelociraptor\n";
  CHECK(resolves(ve1, "ve", "velociraptor mongoliensis", "velociraptor"));

  // The index itself is checked: bad prefix, bad slug, cut, wrong count
  CHECK(!resolves("ZDX1\txx\t1\nxylo\txylo\n", "ty", "xylo", nullptr));
  CHECK(!resolves("ZDX1\tty\t1\ntyrannosaurus\t../evil\n", "ty", "tyrannosaurus", nullptr));
  CHECK(!resolves("ZDX1\tty\t1\ntyrannosaurus\ta/b\n", "ty", "tyrannosaurus", nullptr));
  CHECK(!resolves("ZDX1\tty\t1\ntyrannosaurus\tty..x\n", "ty", "tyrannosaurus", "ty..x"));
  CHECK(!resolves("ZDX1\tty\t2\ntyrannosaurus\ttyrannosaurus\ntyrannot", "ty", "tyrannosaurus", nullptr));
  CHECK(!resolves("ZDX1\tty\t2\ntyrannosaurus\ttyrannosaurus\n", "ty", "tyrannosaurus", nullptr));
  CHECK(!resolves("ZDX1\tty\t1\ntyrannosaurus\ttyrannosaurus", "ty", "tyrannosaurus", nullptr));
  CHECK(!resolves("ZDX2\tty\t1\ntyrannosaurus\ttyrannosaurus\n", "ty", "tyrannosaurus", nullptr));
  CHECK(!resolves("ZDX1\tty\t1\ntyrannosaurus\n", "ty", "tyrannosaurus", nullptr));
  char slug[4];  // Too small for the answer
  std::vector<char> index(ty, ty + strlen(ty) + 1);
  CHECK(!dinoResolve(index.data(), strlen(ty), "ty", "tyrannosaurus", slug, sizeof(slug)));
}

static void testParseGood() {
  DinoSheet s;
  std::vector<char> storage;
  CHECK(parseText(TREX_TEXT, "tyrannosaurus", s, storage));
  CHECK_STR(s.slug, "tyrannosaurus");
  CHECK_STR(s.qid, "Q14332");
  CHECK_STR(s.name, "Tyrannosaurus");
  CHECK(s.group == DINO_DINOSAUR);
  CHECK(s.length.present && s.length.lo == 1200 && s.length.hi == 1300);
  CHECK(s.height.present && s.height.hi == 400);
  CHECK(!s.wingspan.present && !s.egg.present);
  CHECK(s.mass.lo == 8800000 && s.speed.lo == 200 && s.speed.hi == 300);
  CHECK(s.lived.lo == 68000000 && s.lived.hi == 66000000);
  CHECK(s.named == 1905);
  CHECK(s.hasImage && s.imageLength == 9000 && s.imageW == 240 && s.imageH == 117 && s.imageHash == 0x0badf00d);
  CHECK_STR(s.credit, "John Doe, CC BY 2.0, resized");
  CHECK_STR(dinoText(s.era, false), "Kreide");
  CHECK_STR(dinoText(s.fun, true), "Its bite was stronger than any land animal's.");
  CHECK_STR(s.funSource, "Wikipedia (EN): Tyrannosaurus");
  CHECK_STR(s.source, "Wikidata, Wikipedia");
  CHECK(s.diet.de == nullptr && s.found.en == nullptr);

  // Texts fall back to the other language; image "-" means no credit
  DinoText only = {"nur deutsch", nullptr};
  CHECK_STR(dinoText(only, true), "nur deutsch");
  DinoText none = {nullptr, nullptr};
  CHECK_STR(dinoText(none, false), "");

  // A sheet with only the minimum
  CHECK(parseText("ZDS1\tx\tQ1\nname\tX\nend\n", "x", s, storage));
  CHECK(!s.hasImage && s.length.present == false && s.named == 0 && s.group == DINO_DINOSAUR);
  CHECK(parseText(trexWith("John Doe, CC BY 2.0, resized", "-"), "tyrannosaurus", s, storage));
  CHECK_STR(s.credit, "");
  CHECK(parseText(trexWith("group\t0", "group\t2"), "tyrannosaurus", s, storage) && s.group == DINO_PTEROSAUR);
  // Keys in any order
  CHECK(parseText("ZDS1\tx\tQ1\nnamed\t1900\nname\tX\nend\n", "x", s, storage) && s.named == 1900);
}

static void testParseHostile() {
  CHECK(parses(TREX_TEXT));
  std::string t = TREX_TEXT;
  CHECK(!parses(t.substr(0, t.size() - 4)));                      // No end
  CHECK(!parses(t.substr(0, t.size() - 1)));                      // The last line has no line end
  CHECK(!parses(t.substr(0, t.find("length") + 8)));              // Cut in the middle of a line
  CHECK(!parses(t + "name\tAgain\n"));                            // Something after end
  CHECK(!parses(trexWith("tyrannosaurus\tQ14332", "other\tQ14332")));       // Wrong slug
  CHECK(!parses(trexWith("\tQ14332", "\t14332")));                // Not a QID
  CHECK(!parses(trexWith("ZDS1", "ZDS2")));
  CHECK(!parses(trexWith("named\t1905\n", "named\t1905\nnamed\t1906\n")));  // Duplicate key
  CHECK(!parses(trexWith("length\t1200\t1300", "length\t1300\t1200")));     // lo > hi
  CHECK(!parses(trexWith("length\t1200\t1300", "length\t-5\t1300")));       // Negative
  CHECK(!parses(trexWith("length\t1200\t1300", "length\t12.5\t1300")));     // Not a whole number
  CHECK(!parses(trexWith("length\t1200\t1300", "length\t1200")));           // Missing field
  CHECK(!parses(trexWith("lived\t68000000\t66000000", "lived\t66000000\t68000000")));  // Older must come first
  CHECK(!parses(trexWith("group\t0", "group\t4")));
  CHECK(!parses(trexWith("group\t0", "group\t")));
  CHECK(!parses(trexWith("name\tTyrannosaurus", "name\t")));
  CHECK(!parses(trexWith("name\tTyrannosaurus\n", "")));                    // No name
  CHECK(!parses(trexWith("era\tKreide\tCretaceous", "era\tKreide\t")));
  CHECK(!parses(trexWith("era\tKreide\tCretaceous", "era\tKreide")));
  CHECK(!parses(trexWith("era\tKreide\tCretaceous", "era\t" + std::string(DINO_FIELD_LIMIT + 1, 'a') + "\tx")));
  CHECK(parses(trexWith("era\tKreide\tCretaceous", "era\t" + std::string(DINO_FIELD_LIMIT, 'a') + "\tx")));
  CHECK(!parses(trexWith("Kreide", "Kr\x01ide")));                          // Control byte
  CHECK(!parses(trexWith("Kreide", "Kr\xE2\x80\x93ide")));                  // En dash: three bytes
  CHECK(!parses(trexWith("Kreide", "Kr\xC3ide")));                          // Cut character
  CHECK(!parses(trexWith("image\t9000\t240\t117", "image\t9000\t241\t117")));   // Too wide
  CHECK(!parses(trexWith("image\t9000\t240\t117", "image\t9000\t240\t118")));   // Too high
  CHECK(!parses(trexWith("image\t9000\t240\t117", "image\t0\t240\t117")));
  CHECK(!parses(trexWith("image\t9000\t240\t117", "image\t32769\t240\t117")));  // Over DINO_IMAGE_LIMIT
  CHECK(!parses(trexWith("0badf00d", "0BADF00D")));
  CHECK(!parses(trexWith("0badf00d", "0badf00")));
  CHECK(!parses(trexWith("named\t1905", "named\t0")));
  CHECK(parses(trexWith("named\t1905\n", "named\t1905\nfuture\tx\ty\tz\n")));  // Unknown keys are ignored
  CHECK(parses(trexWith("named\t1905\n", "named\t1905\nfuture\n")));
  CHECK(!parses(trexWith("named\t1905\n", "named\t1905\n\n")));             // Empty line

  // Size limits and a NUL inside
  std::string big = "ZDS1\ttyrannosaurus\tQ14332\nname\tT\n";
  while (big.size() < DINO_TEXT_LIMIT + 10) big += "future\tpadding padding padding padding\n";
  CHECK(!parses(big + "end\n"));
  std::string withNul = TREX_TEXT;
  withNul[withNul.find("named") + 2] = 0;
  CHECK(!parses(withNul));
  DinoSheet s;
  std::vector<char> storage(TREX_TEXT, TREX_TEXT + strlen(TREX_TEXT) + 1);
  CHECK(!dinoParseSheet(storage.data(), strlen(TREX_TEXT), strlen(TREX_TEXT), "tyrannosaurus", s));  // No room for a NUL
  CHECK(!dinoParseSheet(nullptr, 5, 10, "x", s));
}

static void testImageChecks() {
  std::vector<uint8_t> jpeg(300, 0x55);
  jpeg[0] = 0xFF;
  jpeg[1] = 0xD8;
  jpeg[298] = 0xFF;
  jpeg[299] = 0xD9;
  DinoSheet s;
  memset(&s, 0, sizeof(s));
  s.hasImage = true;
  s.imageLength = (uint32_t)jpeg.size();
  s.imageHash = dinoFnv1a(jpeg.data(), jpeg.size());
  CHECK(dinoImageOk(s, jpeg.data(), jpeg.size()));
  CHECK(dinoImageOk(s, jpeg.data(), jpeg.size() + 10));       // More arrived than needed
  CHECK(!dinoImageOk(s, jpeg.data(), jpeg.size() - 1));       // Cut
  CHECK(!dinoImageOk(s, nullptr, jpeg.size()));
  s.imageHash ^= 1;
  CHECK(!dinoImageOk(s, jpeg.data(), jpeg.size()));           // Wrong hash
  std::vector<uint8_t> noStart = jpeg;
  noStart[0] = 0;
  s.imageHash = dinoFnv1a(noStart.data(), noStart.size());
  CHECK(!dinoImageOk(s, noStart.data(), noStart.size()));
  std::vector<uint8_t> noEnd = jpeg;
  noEnd[299] = 0;
  s.imageHash = dinoFnv1a(noEnd.data(), noEnd.size());
  CHECK(!dinoImageOk(s, noEnd.data(), noEnd.size()));
  s.hasImage = false;
  s.imageHash = dinoFnv1a(jpeg.data(), jpeg.size());
  CHECK(!dinoImageOk(s, jpeg.data(), jpeg.size()));           // The sheet names no picture
  CHECK(dinoFnv1a((const uint8_t*)"a", 1) == 0xE40C292CUL);
  CHECK(dinoFnv1a(nullptr, 0) == 0x811C9DC5UL);
}

// A credit too wide for the footer is shown as its licence alone: the text after the last ", "
static void testCreditLicence() {
  CHECK_STR(dinoCreditLicence("Steven U. Vidovic, David M. Martill, CC BY 2.5"), "CC BY 2.5");
  CHECK_STR(dinoCreditLicence("Emily Willoughby , (emilywilloughby.com), CC BY 3.0"), "CC BY 3.0");
  CHECK_STR(dinoCreditLicence("CC BY-SA 4.0"), "CC BY-SA 4.0");  // The licence alone already
  CHECK_STR(dinoCreditLicence(""), "");
  CHECK_STR(dinoCreditLicence(nullptr), "");
  CHECK_STR(dinoCreditLicence("Nobu Tamura,CC BY 3.0"), "Nobu Tamura,CC BY 3.0");  // Only ", " separates
}

static void testFormats() {
  char out[48];
  dinoFormatLength(1200, 1300, false, out, sizeof(out));
  CHECK_STR(out, "12-13 m");
  dinoFormatLength(180, 207, false, out, sizeof(out));
  CHECK_STR(out, "1,8-2,07 m");
  dinoFormatLength(180, 207, true, out, sizeof(out));
  CHECK_STR(out, "1.8-2.07 m");
  dinoFormatLength(80, 80, false, out, sizeof(out));
  CHECK_STR(out, "80 cm");
  dinoFormatLength(1200, 1200, false, out, sizeof(out));
  CHECK_STR(out, "12 m");
  dinoFormatLength(50, 80, false, out, sizeof(out));
  CHECK_STR(out, "50-80 cm");
  dinoFormatLength(80, 130, true, out, sizeof(out));
  CHECK_STR(out, "0.8-1.3 m");
  dinoFormatMass(1001000, 1004000, false, out, sizeof(out));  // The same text twice is shown once
  CHECK_STR(out, "1 t");
  dinoFormatLength(3500, 3500, false, out, sizeof(out));
  CHECK_STR(out, "35 m");

  dinoFormatMass(8800000, 8800000, false, out, sizeof(out));
  CHECK_STR(out, "8,8 t");
  dinoFormatMass(8800000, 8800000, true, out, sizeof(out));
  CHECK_STR(out, "8.8 t");
  dinoFormatMass(15000, 15000, true, out, sizeof(out));
  CHECK_STR(out, "15 kg");
  dinoFormatMass(500, 800, false, out, sizeof(out));
  CHECK_STR(out, "500-800 g");
  dinoFormatMass(5500, 5500, false, out, sizeof(out));
  CHECK_STR(out, "5,5 kg");
  dinoFormatMass(5000, 5000, false, out, sizeof(out));
  CHECK_STR(out, "5 kg");
  dinoFormatMass(7000000, 9000000, true, out, sizeof(out));
  CHECK_STR(out, "7-9 t");
  dinoFormatMass(999000, 999000, true, out, sizeof(out));
  CHECK_STR(out, "999 kg");

  dinoFormatSpeed(153, 179, false, out, sizeof(out));
  CHECK_STR(out, "15,3-17,9 km/h");
  dinoFormatSpeed(200, 300, true, out, sizeof(out));
  CHECK_STR(out, "20-30 km/h");
  dinoFormatSpeed(250, 250, true, out, sizeof(out));
  CHECK_STR(out, "25 km/h");

  dinoFormatEgg(60, 80, false, out, sizeof(out));
  CHECK_STR(out, "60-80 mm");
  dinoFormatEgg(450, 450, false, out, sizeof(out));
  CHECK_STR(out, "45 cm");
  dinoFormatEgg(120, 155, true, out, sizeof(out));
  CHECK_STR(out, "12-15.5 cm");

  dinoFormatLived(68000000, 66000000, false, out, sizeof(out));
  CHECK_STR(out, "vor 68-66 Mio. Jahren");
  dinoFormatLived(68000000, 66000000, true, out, sizeof(out));
  CHECK_STR(out, "68-66 million years ago");
  dinoFormatLived(5300000, 2600000, false, out, sizeof(out));
  CHECK_STR(out, "vor 5,3-2,6 Mio. Jahren");
  dinoFormatLived(150000000, 150000000, true, out, sizeof(out));
  CHECK_STR(out, "150 million years ago");
  dinoFormatLived(70000000, 70000000, true, out, sizeof(out));
  CHECK_STR(out, "70 million years ago");

  // Added to the AI's answer when no sheet came; the AI itself never mentions one
  CHECK_STR(dinoMissLine(false), "Einen Steckbrief dazu habe ich nicht.");
  CHECK_STR(dinoMissLine(true), "I don't have a fact sheet for that.");

  char tiny[6];  // Never writes past its size
  dinoFormatLength(1200, 1300, false, tiny, sizeof(tiny));
  CHECK(strlen(tiny) <= 5);
}

static void testGroupNote() {
  CHECK_STR(dinoGroupNote(DINO_DINOSAUR, false), "");
  CHECK_STR(dinoGroupNote(DINO_DINOSAUR, true), "");
  CHECK_STR(dinoGroupNote(DINO_EARLY_BIRD, false), "Urvogel");
  CHECK_STR(dinoGroupNote(DINO_EARLY_BIRD, true), "Early bird");
  CHECK_STR(dinoGroupNote(DINO_PTEROSAUR, false), "Flugsaurier - kein Dinosaurier");
  CHECK_STR(dinoGroupNote(DINO_PTEROSAUR, true), "Pterosaur - not a dinosaur");
  CHECK_STR(dinoGroupNote(DINO_MARINE, false), "Meeresreptil - kein Dinosaurier");
  CHECK_STR(dinoGroupNote(DINO_MARINE, true), "Marine reptile - not a dinosaur");
}

static void testRows() {
  DinoSheet s;
  std::vector<char> storage;
  CHECK(parseText(TREX_TEXT, "tyrannosaurus", s, storage));
  DinoRow rows[DINO_MAX_ROWS];
  int n = dinoFactRows(s, false, rows, DINO_MAX_ROWS);
  const char* labels[] = {"L\xC3\xA4nge", "H\xC3\xB6he", "Gewicht", "Tempo", "Zeit", "Lebte", "Benannt", "Bedeutet"};
  CHECK(n == 8);
  for (int i = 0; i < n && i < 8; i++) CHECK_STR(rows[i].label, labels[i]);
  CHECK_STR(rows[0].value, "12-13 m");
  CHECK_STR(rows[1].value, "3,6-4 m");
  CHECK_STR(rows[2].value, "8,8 t");
  CHECK_STR(rows[3].value, "20-30 km/h, gesch\xC3\xA4tzt");
  CHECK_STR(rows[4].value, "Kreide");
  CHECK_STR(rows[5].value, "vor 68-66 Mio. Jahren");
  CHECK_STR(rows[6].value, "1905");
  CHECK_STR(rows[7].value, "K\xC3\xB6nig der Tyrannenechsen");
  for (int i = 0; i < n; i++) CHECK(strcmp(rows[i].label, "Ei") != 0);

  n = dinoFactRows(s, true, rows, DINO_MAX_ROWS);
  CHECK(n == 8);
  CHECK_STR(rows[0].label, "Length");
  CHECK_STR(rows[1].label, "Height");
  CHECK_STR(rows[1].value, "3.6-4 m");
  CHECK_STR(rows[3].label, "Speed");
  CHECK_STR(rows[3].value, "20-30 km/h, estimated");
  CHECK_STR(rows[5].value, "68-66 million years ago");
  CHECK_STR(rows[7].label, "Means");
  CHECK_STR(rows[7].value, "king of the tyrant lizards");
  CHECK(dinoFactRows(s, true, rows, 3) == 3);  // Never more than asked for

  // Every row, in the order of the table, with the languages falling back to each other
  std::string all =
      "ZDS1\tx\tQ1\nname\tX\nlength\t100\t100\nheight\t100\t100\nwingspan\t100\t100\nmass\t1000\t1000\n"
      "speed\t100\t100\negg\t100\t100\nera\tA\tB\nlived\t5\t5\nnamed\t1900\nmeaning\tM\tN\ndiet\tD\tE\nfound\tF\tG\nend\n";
  CHECK(parseText(all, "x", s, storage));
  n = dinoFactRows(s, true, rows, DINO_MAX_ROWS);
  const char* en[] = {"Length", "Height", "Wingspan", "Weight", "Speed", "Egg", "Era", "Lived", "Named", "Means", "Diet", "Found in"};
  CHECK(n == 12 && DINO_MAX_ROWS == 12);
  for (int i = 0; i < n && i < 12; i++) CHECK_STR(rows[i].label, en[i]);
  n = dinoFactRows(s, false, rows, DINO_MAX_ROWS);
  CHECK_STR(rows[2].label, "Spannweite");
  CHECK_STR(rows[5].label, "Ei");
  CHECK_STR(rows[7].label, "Lebte");
  CHECK_STR(rows[10].label, "Nahrung");
  CHECK_STR(rows[11].label, "Fundort");
  CHECK_STR(rows[11].value, "F");
  CHECK(strlen(rows[0].label) < sizeof(rows[0].label));
}

// A sheet with given picture and number of rows, rows taken from the front of the table
static DinoSheet sheetWith(bool image, int rowCount, bool fun) {
  DinoSheet s;
  memset(&s, 0, sizeof(s));
  s.hasImage = image;
  DinoRange* r[] = {&s.length, &s.height, &s.mass, &s.speed, &s.egg, &s.lived};
  for (int i = 0; i < rowCount && i < 6; i++) {
    r[i]->present = true;
    r[i]->lo = r[i]->hi = 100;
  }
  if (rowCount > 6) s.named = 1900;
  if (rowCount > 7) s.era = {"a", "b"};
  if (fun) s.fun = {"de", "en"};
  return s;
}

static void testWorthShowing() {
  CHECK(dinoWorthShowing(sheetWith(true, 0, false)));
  CHECK(dinoWorthShowing(sheetWith(false, 2, false)));
  CHECK(!dinoWorthShowing(sheetWith(false, 1, false)));
  CHECK(!dinoWorthShowing(sheetWith(false, 0, true)));
  CHECK(!dinoWorthShowing(sheetWith(false, 0, false)));
  CHECK(dinoWorthShowing(sheetWith(false, 8, true)));
}

static int measureText(const char* text, void*) { return textWidth(text); }

static int linesOf(const DinoSheet& s, bool english, int width, DinoLine* lines) {
  DinoRow rows[DINO_MAX_ROWS];
  const int n = dinoFactRows(s, english, rows, DINO_MAX_ROWS);
  return dinoFactLines(rows, n, width, measureText, nullptr, lines, DINO_MAX_LINES);
}

static DinoRow rowOf(const char* label, const char* value) {
  DinoRow r;
  snprintf(r.label, sizeof(r.label), "%s", label);
  snprintf(r.value, sizeof(r.value), "%s", value);
  return r;
}

static void testFactLines() {
  DinoLine lines[DINO_MAX_LINES];
  const int W = 232;
  // A short row stays one line
  DinoRow one[] = {rowOf("Length", "12-13 m")};
  CHECK(dinoFactLines(one, 1, W, measureText, nullptr, lines, DINO_MAX_LINES) == 1);
  CHECK_STR(lines[0].label, "Length");
  CHECK_STR(lines[0].value, "12-13 m");

  // "68-66 million years ago" next to "Lived" wraps, with the label only on the first line
  DinoRow lived[] = {rowOf("Lived", "68-66 million years ago")};
  CHECK(textWidth("68-66 million years ago") + textWidth("Lived") + 6 > W);
  CHECK(dinoFactLines(lived, 1, W, measureText, nullptr, lines, DINO_MAX_LINES) == 2);
  CHECK_STR(lines[0].label, "Lived");
  CHECK_STR(lines[1].label, "");
  CHECK_STR(lines[0].value, "68-66 million years");
  CHECK_STR(lines[1].value, "ago");

  // A value too long for two lines is dropped, the rows around it stay
  std::string longText;
  for (int i = 0; i < 12; i++) longText += "wordy words ";
  DinoRow three[] = {rowOf("Length", "12 m"), rowOf("Diet", longText.c_str()), rowOf("Era", "Jurassic")};
  CHECK(dinoFactLines(three, 3, W, measureText, nullptr, lines, DINO_MAX_LINES) == 2);
  CHECK_STR(lines[0].label, "Length");
  CHECK_STR(lines[1].label, "Era");
  // One long word that fits no line is dropped as well
  DinoRow word[] = {rowOf("Diet", std::string(60, 'W').c_str())};
  CHECK(dinoFactLines(word, 1, W, measureText, nullptr, lines, DINO_MAX_LINES) == 0);
  // After a hyphen is a break, too
  DinoRow hyphen[] = {rowOf("Found in", "Nordamerika-Asien-Afrika")};
  CHECK(dinoFactLines(hyphen, 1, W, measureText, nullptr, lines, DINO_MAX_LINES) == 2);
  CHECK(lines[0].value[strlen(lines[0].value) - 1] == '-');

  // The label column follows the widest label of the sheet
  DinoRow narrow[] = {rowOf("Era", "Jurassic Cretaceous Triassic Jurassic")};
  DinoRow wide[] = {rowOf("Era", "Jurassic Cretaceous Triassic Jurassic"), rowOf("Wingspan", "x")};
  const int w1 = textWidth("Jurassic Cretaceous Triassic Jurassic");
  const int width = textWidth("Era") + 6 + w1;  // Just wide enough beside the short label
  CHECK(dinoFactLines(narrow, 1, width, measureText, nullptr, lines, DINO_MAX_LINES) == 1);
  CHECK(dinoFactLines(wide, 2, width, measureText, nullptr, lines, DINO_MAX_LINES) == 3);

  // Never inside a UTF-8 character: every line is valid device text, all cut points are at spaces
  std::string value;
  for (int i = 0; i < 12; i++) value += "\xC3\xA4";
  value += " \xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4";
  DinoRow umlaut[] = {rowOf("L\xC3\xA4nge", value.c_str())};
  const int n = dinoFactLines(umlaut, 1, W, measureText, nullptr, lines, DINO_MAX_LINES);
  CHECK(n == 2);
  for (int i = 0; i < n; i++) CHECK(dinoValidText(lines[i].value, DINO_FIELD_LIMIT, false));
  // No room at all, and the line limit
  CHECK(dinoFactLines(one, 1, 20, measureText, nullptr, lines, DINO_MAX_LINES) == 0);
  CHECK(dinoFactLines(lived, 1, W, measureText, nullptr, lines, 1) == 0);  // Two lines do not fit in one
}

static void testWrapText() {
  char lines[DINO_FUN_LINES][DINO_FIELD_LIMIT + 1];
  const char* fun = "Tyrannosaurus had the strongest estimated bite of all land animals.";
  const int n = dinoWrapText(fun, 224, measureText, nullptr, &lines[0][0], DINO_FUN_LINES, sizeof(lines[0]));
  CHECK(n == 3);
  CHECK(n == wrapLines(fun, 224));  // The same lines as the reference wrap
  CHECK_STR(lines[0], "Tyrannosaurus had the");
  CHECK_STR(lines[1], "strongest estimated bite");
  CHECK_STR(lines[2], "of all land animals.");
  for (int i = 0; i < n; i++) CHECK(textWidth(lines[i]) <= 224);
  // Only counting
  CHECK(dinoWrapText(fun, 224, measureText, nullptr, nullptr, 0, 0) == 3);
  // One line, extra spaces fold away
  CHECK(dinoWrapText("  Short   one ", 224, measureText, nullptr, &lines[0][0], DINO_FUN_LINES, sizeof(lines[0])) == 1);
  CHECK_STR(lines[0], "Short one");
  CHECK(dinoWrapText("", 224, measureText, nullptr, &lines[0][0], DINO_FUN_LINES, sizeof(lines[0])) == 0);
  // A word wider than a line stands alone
  const std::string wide(40, 'W');
  const std::string withWide = "a " + wide + " b";
  CHECK(dinoWrapText(withWide.c_str(), 224, measureText, nullptr, &lines[0][0], DINO_FUN_LINES, sizeof(lines[0])) == 3);
  CHECK_STR(lines[0], "a");
  CHECK_STR(lines[1], wide.c_str());
  CHECK_STR(lines[2], "b");
  // More lines than there is room for: the last one shown ends in "..." and still fits
  std::string longText;
  for (int i = 0; i < 20; i++) longText += "words and ";
  const int needed = dinoWrapText(longText.c_str(), 224, measureText, nullptr, &lines[0][0], DINO_FUN_LINES,
                                  sizeof(lines[0]));
  CHECK(needed > DINO_FUN_LINES);
  CHECK(needed == wrapLines(longText.c_str(), 224));
  const char* last = lines[DINO_FUN_LINES - 1];
  CHECK(strlen(last) > 3 && strcmp(last + strlen(last) - 3, "...") == 0);
  CHECK(last[strlen(last) - 4] != ' ');
  CHECK(textWidth(last) <= 224);
  // Umlauts are measured and kept whole
  const char* umlauts =
      "Die gr\xC3\xB6\xC3\x9F" "ten Eier waren gr\xC3\xB6\xC3\x9F" "er als ein Fussball und schwerer als ein Ziegelstein.";
  const int u = dinoWrapText(umlauts, 224, measureText, nullptr, &lines[0][0], DINO_FUN_LINES, sizeof(lines[0]));
  CHECK(u == wrapLines(umlauts, 224));
  for (int i = 0; i < u && i < DINO_FUN_LINES; i++) CHECK(dinoValidText(lines[i], DINO_FIELD_LIMIT, false));
}

static void testPages() {
  DinoSheet s;
  std::vector<char> storage;
  CHECK(parseText(TREX_TEXT, "tyrannosaurus", s, storage));
  DinoLine lines[DINO_MAX_LINES];
  DinoPageRef pages[DINO_MAX_PAGES];
  int lineCount = linesOf(s, false, 232, lines);
  CHECK(lineCount >= 5);
  int n = dinoPages(s, lines, lineCount, pages, DINO_MAX_PAGES);
  const int factsPages = (lineCount + DINO_LINES_PER_PAGE - 1) / DINO_LINES_PER_PAGE;
  CHECK(n >= 1 + factsPages + 2 && n <= DINO_MAX_PAGES);
  CHECK(pages[0].kind == DINO_PAGE_PICTURE);
  CHECK(pages[n - 2].kind == DINO_PAGE_SIZE && pages[n - 1].kind == DINO_PAGE_FUN);
  // The facts pages together cover every line once, in order, and never part a row
  int next = 0;
  for (int i = 1; i < n - 2; i++) {
    CHECK(pages[i].kind == DINO_PAGE_FACTS);
    CHECK(pages[i].firstLine == next);
    CHECK(pages[i].lineCount >= 1 && pages[i].lineCount <= DINO_LINES_PER_PAGE);
    CHECK(lines[pages[i].firstLine].label[0] != 0);  // A page starts with a row's first line
    next += pages[i].lineCount;
  }
  CHECK(next == lineCount);

  // Ten one-line rows are two pages of five; nothing wraps
  DinoLine ten[DINO_MAX_LINES];
  for (int i = 0; i < 10; i++) {
    snprintf(ten[i].label, sizeof(ten[i].label), "L%d", i);
    snprintf(ten[i].value, sizeof(ten[i].value), "v");
  }
  DinoSheet bare = sheetWith(false, 0, true);
  n = dinoPages(bare, ten, 10, pages, DINO_MAX_PAGES);
  CHECK(n == 3);  // No picture, no length: facts, facts, fun
  if (n == 3) {
    CHECK(pages[0].kind == DINO_PAGE_FACTS && pages[0].firstLine == 0 && pages[0].lineCount == 5);
    CHECK(pages[1].kind == DINO_PAGE_FACTS && pages[1].firstLine == 5 && pages[1].lineCount == 5);
    CHECK(pages[2].kind == DINO_PAGE_FUN);
  }
  // A row whose second line would open a page starts the page instead
  ten[5].label[0] = 0;  // Line 5 continues line 4
  n = dinoPages(bare, ten, 10, pages, DINO_MAX_PAGES);
  CHECK(n == 4);  // Facts 0-3, facts 4-8, facts 9, fun
  if (n == 4) {
    CHECK(pages[0].lineCount == 4 && pages[1].firstLine == 4 && pages[1].lineCount == 5 && pages[1].kind == DINO_PAGE_FACTS);
    CHECK(pages[2].firstLine == 9 && pages[2].lineCount == 1);
  }

  // No lines: no facts page
  DinoSheet eight = sheetWith(true, 8, false);
  eight.length = {100, 100, true};
  n = dinoPages(eight, ten, 0, pages, DINO_MAX_PAGES);
  CHECK(n == 2 && pages[0].kind == DINO_PAGE_PICTURE && pages[1].kind == DINO_PAGE_SIZE);
  CHECK(dinoPages(eight, ten, 10, pages, 2) == 2);  // Never more than asked for
  // A wingspan alone gets a size page too
  DinoSheet wing = sheetWith(true, 0, false);
  wing.wingspan = {600, 600, true};
  CHECK(dinoPages(wing, ten, 0, pages, DINO_MAX_PAGES) == 2 && pages[1].kind == DINO_PAGE_SIZE);
  CHECK(dinoPages(sheetWith(false, 0, false), ten, 0, pages, DINO_MAX_PAGES) == 0);
}

static bool inside(int x, int y, int w, int h, int areaW, int areaH) {
  return x >= 0 && y >= 0 && w >= 1 && h >= 1 && x + w <= areaW && y + h <= areaH;
}

static bool layoutInside(const DinoSizeLayout& l, int areaW, int areaH) {
  return inside(l.personX, l.personY, l.personW, l.personH, areaW, areaH) &&
         inside(l.bodyX, l.bodyY, l.bodyW, l.bodyH, areaW, areaH) && l.rangeW >= 0 && l.rangeW <= l.bodyW;
}

static void testSizeLayout() {
  const int W = 224, H = 79;
  DinoSheet s;
  std::vector<char> storage;
  CHECK(parseText(TREX_TEXT, "tyrannosaurus", s, storage));
  DinoSizeLayout l;
  CHECK(dinoSizeLayout(s, W, H, l));
  CHECK(!l.lineOnly && !l.wingspan);
  CHECK(abs(l.personH - (int)lround(l.bodyH * 170.0 / 400.0)) <= 1);
  CHECK(layoutInside(l, W, H));
  CHECK(l.personX + l.personW <= l.bodyX);  // They stand side by side
  CHECK(l.rangeW > 0 && l.rangeW < l.bodyW);

  // 80 cm, height 50: smaller than the person
  DinoSheet small = sheetWith(false, 0, false);
  small.length = {80, 80, true};
  small.height = {50, 50, true};
  CHECK(dinoSizeLayout(small, W, H, l));
  CHECK(l.personH > l.bodyH);
  CHECK(layoutInside(l, W, H));
  CHECK(l.rangeW == 0);

  // 3500 cm, no height
  DinoSheet longest = sheetWith(false, 0, false);
  longest.length = {3000, 3500, true};
  CHECK(dinoSizeLayout(longest, W, H, l));
  CHECK(l.lineOnly && !l.wingspan);
  CHECK(l.personH >= 2);
  CHECK(l.bodyH == 5);
  CHECK(layoutInside(l, W, H));

  // Wingspan only
  DinoSheet wing = sheetWith(false, 0, false);
  wing.wingspan = {1000, 1000, true};
  CHECK(dinoSizeLayout(wing, W, H, l));
  CHECK(l.wingspan && l.lineOnly);
  CHECK(layoutInside(l, W, H));
  // A wingspan with a height still draws the span as a line, and a length wins over a wingspan
  wing.height = {200, 200, true};
  CHECK(dinoSizeLayout(wing, W, H, l) && l.wingspan && l.lineOnly);
  wing.length = {500, 500, true};
  CHECK(dinoSizeLayout(wing, W, H, l) && !l.wingspan && !l.lineOnly);

  // Neither
  DinoSheet neither = sheetWith(false, 0, false);
  CHECK(!dinoSizeLayout(neither, W, H, l));
  neither.height = {200, 200, true};
  CHECK(!dinoSizeLayout(neither, W, H, l));
  CHECK(!dinoSizeLayout(small, 4, 4, l));  // No room to draw anything

  // Height greater than length: tall and thin, still inside
  DinoSheet tall = sheetWith(false, 0, false);
  tall.length = {300, 300, true};
  tall.height = {1800, 1800, true};
  CHECK(dinoSizeLayout(tall, W, H, l));
  CHECK(layoutInside(l, W, H));
  CHECK(l.personH < l.bodyH);

  // Extremes inside areas of other sizes, with the person always at least 2 px
  int lengths[] = {1, 80, 169, 170, 171, 1300, 3500, 12000, 100000};
  int heights[] = {0, 1, 50, 170, 400, 1800, 100000};
  int areas[][2] = {{224, 79}, {240, 117}, {100, 40}, {30, 10}};
  for (auto& a : areas) {
    for (int len : lengths) {
      for (int hei : heights) {
        DinoSheet e = sheetWith(false, 0, false);
        e.length = {len, len, true};
        if (hei) e.height = {hei, hei, true};
        CHECK(dinoSizeLayout(e, a[0], a[1], l));
        CHECK(layoutInside(l, a[0], a[1]));
        CHECK(l.personH >= 2);
        e.length = {len, len + len / 3, true};
        CHECK(dinoSizeLayout(e, a[0], a[1], l) && layoutInside(l, a[0], a[1]));
      }
    }
  }
}

static bool endsWith(const std::string& s, const char* tail) {
  const size_t n = strlen(tail);
  return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

static void testGeneratedFiles(const std::string& dir) {
  DIR* d = opendir(dir.c_str());
  if (!d) {
    printf("Generated files skipped: %s does not exist yet\n", dir.c_str());
    return;
  }
  std::vector<std::string> indexes;
  std::set<std::string> jpgs, pictured;  // Slugs of the .jpg files, and of the sheets that name one
  while (dirent* e = readdir(d)) {
    std::string name = e->d_name;
    if (name.rfind("index-", 0) == 0 && endsWith(name, ".txt")) indexes.push_back(name);
    if (endsWith(name, ".jpg")) jpgs.insert(name.substr(0, name.size() - 4));
  }
  closedir(d);
  CHECK(!indexes.empty());
  int sheets = 0, droppedRows = 0;
  for (const std::string& file : indexes) {
    std::string text = readFile(dir + "/" + file);
    CHECK(text.size() <= DINO_INDEX_LIMIT);
    const std::string prefix = file.substr(6, 2);
    size_t at = text.find('\n');
    CHECK(at != std::string::npos);
    while (at != std::string::npos && at + 1 < text.size()) {
      const size_t end = text.find('\n', at + 1);
      if (end == std::string::npos) break;
      const std::string line = text.substr(at + 1, end - at - 1);
      at = end;
      const size_t tab = line.find('\t');
      CHECK(tab != std::string::npos);
      if (tab == std::string::npos) continue;
      const std::string name = line.substr(0, tab), slug = line.substr(tab + 1);

      std::vector<char> index(text.begin(), text.end());
      index.push_back(0);
      char found[48];
      CHECK(dinoResolve(index.data(), text.size(), prefix.c_str(), name.c_str(), found, sizeof(found)));
      CHECK(slug == found);

      std::string sheetText = readFile(dir + "/" + slug + ".txt");
      CHECK(!sheetText.empty() && sheetText.size() <= DINO_TEXT_LIMIT);
      DinoSheet s;
      std::vector<char> storage;
      const bool ok = parseText(sheetText, slug.c_str(), s, storage);
      CHECK(ok);
      if (!ok) {
        printf("  in %s.txt\n", slug.c_str());
        continue;
      }
      sheets++;
      CHECK(dinoWorthShowing(s));
      // A name wider than the line is drawn in DejaVu12, which has only ASCII
      if (textWidth(s.name) > 232) {
        bool ascii = true;
        for (const char* c = s.name; *c; c++) ascii = ascii && (unsigned char)*c < 0x80;
        CHECK(ascii);
        if (!ascii) printf("  %s: the name is too wide and not ASCII\n", slug.c_str());
      }
      // The small footer font is ASCII only, and the parser doesn't enforce that
      for (const char* footer : {s.source, s.credit, s.funSource}) {
        for (const char* c = footer; *c; c++) CHECK((unsigned char)*c >= 0x20 && (unsigned char)*c < 0x7F);
      }
      if (s.hasImage && s.credit[0]) {
        // What the footer falls back to when the credit is too wide: never empty, and ASCII for DejaVu9
        const char* licence = dinoCreditLicence(s.credit);
        bool ascii = true;
        for (const char* c = licence; *c; c++) ascii = ascii && (unsigned char)*c >= 0x20 && (unsigned char)*c < 0x7F;
        CHECK(licence[0] && ascii);
        if (!licence[0] || !ascii) printf("  %s: no licence in the credit '%s'\n", slug.c_str(), s.credit);
      }
      if (s.hasImage) {
        // The folder exists, so a sheet that names a picture must have it, byte for byte
        CHECK(s.imageLength <= DINO_IMAGE_LIMIT);
        const std::string jpeg = readFile(dir + "/" + slug + ".jpg");
        const bool whole = !jpeg.empty() && jpeg.size() == s.imageLength &&
                           dinoImageOk(s, (const uint8_t*)jpeg.data(), jpeg.size());
        CHECK(whole);
        if (!whole) printf("  %s.jpg is missing or not the picture its sheet names\n", slug.c_str());
        pictured.insert(slug);
      }
      for (int english = 0; english < 2; english++) {
        // No row is dropped: each fits two lines at 240 - 2 x 4 px, and the pages fit DINO_MAX_PAGES
        DinoRow rows[DINO_MAX_ROWS];
        const int n = dinoFactRows(s, english != 0, rows, DINO_MAX_ROWS);
        DinoLine lines[DINO_MAX_LINES];
        const int lineCount = dinoFactLines(rows, n, 232, measureText, nullptr, lines, DINO_MAX_LINES);
        int rowsShown = 0;
        for (int i = 0; i < lineCount; i++) rowsShown += lines[i].label[0] ? 1 : 0;
        CHECK(rowsShown == n);
        if (rowsShown != n) {
          droppedRows += n - rowsShown;
          for (int i = 0; i < n; i++) {
            bool shown = false;
            for (int k = 0; k < lineCount; k++) shown = shown || strcmp(lines[k].label, rows[i].label) == 0;
            if (!shown) printf("  %s %s dropped: %s\n", slug.c_str(), rows[i].label, rows[i].value);
          }
        }
        DinoPageRef pages[DINO_MAX_PAGES + 4];
        const int pageCount = dinoPages(s, lines, lineCount, pages, DINO_MAX_PAGES + 4);
        CHECK(pageCount <= DINO_MAX_PAGES);
        if (pageCount > DINO_MAX_PAGES) printf("  %s needs %d pages\n", slug.c_str(), pageCount);
        // The fun fact as feature_dino.ino wraps it: Roboto 10pt, 224 px, DINO_FUN_LINES lines
        const char* fun = dinoText(s.fun, english != 0);
        if (fun[0]) {
          const int needed = dinoWrapText(fun, 224, measureText, nullptr, nullptr, 0, 0);
          CHECK(needed == wrapLines(fun, 224));
          const bool wraps = needed <= DINO_FUN_LINES;
          CHECK(wraps);
          if (!wraps) printf("  %s fun fact is too long\n", slug.c_str());
        }
      }
    }
  }
  // No picture without a sheet that names it: it would be uploaded for nothing
  for (const std::string& slug : jpgs) {
    CHECK(pictured.count(slug));
    if (!pictured.count(slug)) printf("  %s.jpg belongs to no sheet\n", slug.c_str());
  }
  printf("Generated files: %d sheets in %d indexes, %d pictures, %d rows dropped\n", sheets, (int)indexes.size(),
         (int)pictured.size(), droppedRows);
}

int main(int argc, char** argv) {
  testIndexName();
  testResolveForms();
  testParseGood();
  testParseHostile();
  testImageChecks();
  testCreditLicence();
  testFormats();
  testGroupNote();
  testRows();
  testWorthShowing();
  testFactLines();
  testWrapText();
  testPages();
  testSizeLayout();
  testGeneratedFiles(argc > 1 ? argv[1] : "");
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
