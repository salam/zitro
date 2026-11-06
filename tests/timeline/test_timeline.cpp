// Host tests for timeline_core.h. Fixtures are the generated files in webpage2/data/timeline.
#include <dirent.h>

#include <string>
#include <vector>

#include "../../sketch/sketch_jan12a/timeline_core.h"

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

static bool parseText(std::string text, const char* id, Timeline& t, std::vector<char>& storage) {
  storage.assign(text.begin(), text.end());
  storage.resize(text.size() + 1);
  return timelineParse(storage.data(), text.size(), storage.size(), id, t);
}

static const char* GOOD =
    "ZTL1\tdemo\tQ1\tTitel\tTitle\t3\t100\n"
    "Q10\t1914\t1918\t0\tErster\tFirst\t0\t40\t100\t96\t00000000\t-\n"
    "Q11\t1939\t-\t1\tZweiter\tSecond\t40\t60\t240\t96\t00000000\tSomeone, CC BY 2.0, resized\n"
    "Q12\t1945\t-\t0\tDritter \xC3\xA4\tThird\t0\t0\t0\t0\t-\t-\n";

static void testParseGood() {
  Timeline t;
  std::vector<char> s;
  CHECK(parseText(GOOD, "demo", t, s));
  CHECK(t.eventCount == 3);
  CHECK_STR(t.titleDe, "Titel");
  CHECK_STR(t.subject, "Q1");
  CHECK(t.bundleSize == 100);
  CHECK_STR(t.events[0].qid, "Q10");
  CHECK(t.events[0].year == 1914 && t.events[0].endYear == 1918 && !t.events[0].circa);
  CHECK(t.events[1].circa && t.events[1].endYear == 0);
  CHECK_STR(t.events[1].credit, "Someone, CC BY 2.0, resized");
  CHECK_STR(t.events[0].credit, "");
  CHECK(t.events[0].hasImage && t.events[1].hasImage && !t.events[2].hasImage);
  CHECK_STR(t.events[2].labelDe, "Dritter \xC3\xA4");
}

static std::string replaced(std::string text, const std::string& from, const std::string& to) {
  const size_t at = text.find(from);
  if (at == std::string::npos) {
    printf("FAIL test bug, not found: %s\n", from.c_str());
    failures++;
    return text;
  }
  return text.replace(at, from.size(), to);
}

static void testParseHostile() {
  Timeline t;
  std::vector<char> s;
  const std::string good = GOOD;
  CHECK(!parseText("", "demo", t, s));
  CHECK(!parseText("garbage", "demo", t, s));
  CHECK(!parseText("<html>404 not found</html>\n", "demo", t, s));
  CHECK(!parseText(good, "other", t, s));                            // Another topic's file
  CHECK(!parseText(replaced(good, "ZTL1", "ZTL2"), "demo", t, s));   // Unknown format
  CHECK(!parseText(replaced(good, "\t3\t100", "\t4\t100"), "demo", t, s));  // Says more events than it has
  CHECK(!parseText(replaced(good, "\t3\t100", "\t2\t100"), "demo", t, s));  // Says fewer
  CHECK(!parseText(replaced(good, "\t3\t100", "\t0\t100"), "demo", t, s));
  CHECK(!parseText(replaced(good, "\t3\t100", "\t-1\t100"), "demo", t, s));
  CHECK(!parseText(replaced(good, "\t3\t100", "\t999\t100"), "demo", t, s));
  CHECK(!parseText(replaced(good, "\t3\t100", "\t3\t99999999"), "demo", t, s));  // Huge bundle
  CHECK(!parseText(good.substr(0, good.size() - 30), "demo", t, s));  // Cut off in the last line
  CHECK(!parseText(good.substr(0, good.find("Q12")), "demo", t, s));  // Cut off between lines
  CHECK(!parseText(replaced(good, "Q10", "X10"), "demo", t, s));
  CHECK(!parseText(replaced(good, "\t1939\t", "\t1900\t"), "demo", t, s));  // Out of order
  CHECK(!parseText(replaced(good, "\t1939\t", "\t0\t"), "demo", t, s));     // No year 0
  CHECK(!parseText(replaced(good, "\t1939\t", "\t19x9\t"), "demo", t, s));
  CHECK(!parseText(replaced(good, "\t1939\t", "\t 1939\t"), "demo", t, s));
  CHECK(!parseText(replaced(good, "\t1939\t", "\t\t"), "demo", t, s));
  CHECK(!parseText(replaced(good, "\t1939\t", "\t99999999999999999999\t"), "demo", t, s));
  CHECK(!parseText(replaced(good, "\t1914\t1918\t", "\t1914\t1900\t"), "demo", t, s));  // Ends before it starts
  CHECK(!parseText(replaced(good, "\tErster\t", "\t\t"), "demo", t, s));    // Empty label
  CHECK(!parseText(replaced(good, "\tErster\t", std::string("\t") + std::string(200, 'A') + "\t"), "demo", t, s));
  CHECK(!parseText(replaced(good, "\tErster\t", "\t\x01\x02\t"), "demo", t, s));   // Control characters
  CHECK(!parseText(replaced(good, "\tErster\t", "\t\xC3\t"), "demo", t, s));       // Broken UTF-8
  CHECK(!parseText(replaced(good, "\tErster\t", "\t\xE2\x80\x93\t"), "demo", t, s));  // Beyond the font
  CHECK(!parseText(replaced(good, "\tErster\t", "\tA\tB\t"), "demo", t, s));   // Field too many
  CHECK(!parseText(replaced(good, "\t00000000\t-\n", "\t-\n"), "demo", t, s));  // Field missing
  // A NUL inside the text
  std::string nul = good;
  nul[20] = 0;
  CHECK(!parseText(nul, "demo", t, s));
  // No room for the terminating byte
  std::vector<char> tight(good.begin(), good.end());
  CHECK(!timelineParse(tight.data(), tight.size(), tight.size(), "demo", t));
  CHECK(!timelineParse(nullptr, 10, 11, "demo", t));
  // Pictures that can't be right lose the picture and keep the event
  CHECK(parseText(replaced(good, "\t40\t60\t240\t96\t", "\t90\t60\t240\t96\t"), "demo", t, s));  // Past the end
  CHECK(!t.events[1].hasImage && t.eventCount == 3);
  CHECK(parseText(replaced(good, "\t40\t60\t240\t96\t", "\t40\t60\t999\t96\t"), "demo", t, s) == false);  // Too wide
  CHECK(parseText(replaced(good, "\t40\t60\t240\t96\t", "\t40\t60\t240\t96\t"), "demo", t, s));
  CHECK(parseText(replaced(good, "\t40\t60\t240\t96\t00000000", "\t40\t60\t240\t96\tZZ"), "demo", t, s));
  CHECK(!t.events[1].hasImage);
}

static void testImages() {
  std::vector<uint8_t> bundle(100, 0xAA);
  const auto makeJpeg = [&](size_t at, size_t len) {
    bundle[at] = 0xFF;
    bundle[at + 1] = 0xD8;
    bundle[at + len - 2] = 0xFF;
    bundle[at + len - 1] = 0xD9;
  };
  makeJpeg(0, 40);
  makeJpeg(40, 60);
  char hashA[16], hashB[16];
  snprintf(hashA, sizeof(hashA), "%08x", timelineFnv1a(bundle.data(), 40));
  snprintf(hashB, sizeof(hashB), "%08x", timelineFnv1a(bundle.data() + 40, 60));
  std::string text = GOOD;
  text = replaced(text, "\t0\t40\t100\t96\t00000000", std::string("\t0\t40\t100\t96\t") + hashA);
  text = replaced(text, "\t40\t60\t240\t96\t00000000", std::string("\t40\t60\t240\t96\t") + hashB);
  Timeline t;
  std::vector<char> s;
  CHECK(parseText(text, "demo", t, s));
  CHECK(timelineCheckImages(t, bundle.data(), bundle.size()) == 2);
  CHECK(t.events[0].hasImage && t.events[1].hasImage);
  // A slow download: the pictures that arrived whole stay
  CHECK(parseText(text, "demo", t, s));
  CHECK(timelineCheckImages(t, bundle.data(), 70) == 1);
  CHECK(t.events[0].hasImage && !t.events[1].hasImage);
  // Nothing arrived
  CHECK(parseText(text, "demo", t, s));
  CHECK(timelineCheckImages(t, nullptr, 0) == 0);
  CHECK(t.eventCount == 3);
  // One changed byte: a wrong picture is never shown beside an event
  CHECK(parseText(text, "demo", t, s));
  bundle[10] ^= 1;
  CHECK(timelineCheckImages(t, bundle.data(), bundle.size()) == 1);
  CHECK(!t.events[0].hasImage && t.events[1].hasImage);
  bundle[10] ^= 1;
  // Not a JPEG, hash matching
  std::vector<uint8_t> notJpeg(100, 0x41);
  snprintf(hashA, sizeof(hashA), "%08x", timelineFnv1a(notJpeg.data(), 40));
  text = replaced(GOOD, "\t0\t40\t100\t96\t00000000", std::string("\t0\t40\t100\t96\t") + hashA);
  CHECK(parseText(text, "demo", t, s));
  CHECK(timelineCheckImages(t, notJpeg.data(), notJpeg.size()) == 0);
}

static void testTopics() {
  CHECK(timelineIsTopic("world_wars"));
  CHECK(!timelineIsTopic("World_Wars"));
  CHECK(!timelineIsTopic(""));
  CHECK(!timelineIsTopic(nullptr));
  CHECK(!timelineIsTopic("../../etc/passwd"));
  CHECK(!timelineIsTopic("world_wars.txt"));
  CHECK(!timelineIsTopic("world_wars\n"));
}

static void testYears() {
  char big[40], small[24];
  const auto f = [&](int32_t y, int32_t e, bool c, bool en) {
    timelineFormatYear(y, e, c, en, big, sizeof(big), small, sizeof(small));
  };
  f(1969, 0, false, false);
  CHECK_STR(big, "1969");
  CHECK_STR(small, "");
  f(1914, 1918, false, true);
  CHECK_STR(big, "1914-1918");
  f(-44, 0, false, false);
  CHECK_STR(big, "44");
  CHECK_STR(small, "v. Chr.");
  f(-44, 0, false, true);
  CHECK_STR(small, "BC");
  f(-753, -509, false, true);
  CHECK_STR(big, "753-509");
  CHECK_STR(small, "BC");
  f(-27, 395, false, false);
  CHECK_STR(big, "27 v. Chr.-395");
  CHECK_STR(small, "");
  f(-66000000, 0, false, false);
  CHECK_STR(big, "66");
  CHECK_STR(small, "Mio. Jahre her");
  f(-66000000, 0, true, true);
  CHECK_STR(big, "c. 66");
  CHECK_STR(small, "million years ago");
  f(-251902000, -201400000, true, false);
  CHECK_STR(big, "ca. 252-201");
  f(-7200000, 0, false, false);
  CHECK_STR(big, "7,2");
  f(-7200000, 0, false, true);
  CHECK_STR(big, "7.2");
  f(-999999, 0, false, true);
  CHECK_STR(big, "999999");
  CHECK_STR(small, "BC");
  f(INT32_MIN + 1, 0, false, true);  // Must not overflow anything
  CHECK(strlen(big) > 0);

  char span[32];
  timelineFormatSpan(1918, 1939, false, span, sizeof(span));
  CHECK_STR(span, "21 Jahre");
  timelineFormatSpan(1918, 1919, false, span, sizeof(span));
  CHECK_STR(span, "1 Jahr");
  timelineFormatSpan(1918, 1919, true, span, sizeof(span));
  CHECK_STR(span, "1 year");
  timelineFormatSpan(-44, 79, true, span, sizeof(span));  // No year 0 between them
  CHECK_STR(span, "122 years");
  timelineFormatSpan(-68000000, -66000000, false, span, sizeof(span));
  CHECK_STR(span, "2,0 Mio. Jahre");
  timelineFormatSpan(-251902000, -201400000, true, span, sizeof(span));
  CHECK_STR(span, "51 million years");
}

static void testLayout() {
  // Equal time is equal distance; order and minimum gap hold
  Timeline t;
  std::vector<char> s;
  const std::string text =
      "ZTL1\tdemo\tQ1\tA\tB\t6\t0\n"
      "Q1\t1900\t-\t0\ta\ta\t0\t0\t0\t0\t-\t-\n"
      "Q2\t1910\t-\t0\tb\tb\t0\t0\t0\t0\t-\t-\n"
      "Q3\t1920\t-\t0\tc\tc\t0\t0\t0\t0\t-\t-\n"
      "Q4\t1920\t-\t0\td\td\t0\t0\t0\t0\t-\t-\n"
      "Q5\t1940\t-\t0\te\te\t0\t0\t0\t0\t-\t-\n"
      "Q6\t2900\t-\t0\tf\tf\t0\t0\t0\t0\t-\t-\n";
  CHECK(parseText(text, "demo", t, s));
  for (int i = 0; i + 1 < t.eventCount; i++) CHECK(t.events[i + 1].bandX > t.events[i].bandX);
  CHECK(fabsf((t.events[1].bandX - t.events[0].bandX) - (t.events[2].bandX - t.events[1].bandX)) < 0.01f);
  CHECK(t.events[4].bandX - t.events[3].bandX > t.events[1].bandX - t.events[0].bandX);  // 20 years > 10 years
  CHECK(t.events[3].bandX - t.events[2].bandX >= TIMELINE_CARD_SPACE + TIMELINE_GAP_MIN);  // Same year stays apart
  CHECK(!t.events[0].breakAfter && t.events[4].breakAfter);  // 960 years got shortened, and that's marked
  CHECK(t.events[5].bandX - t.events[4].bandX <= TIMELINE_CARD_SPACE + TIMELINE_GAP_MAX + 0.01f);
  CHECK(t.tickYears >= 1);

  // Year on the band
  int exact = -2;
  CHECK(timelineYearAt(t, t.events[1].bandX, exact) == 1910 && exact == 1);
  CHECK(timelineYearAt(t, t.events[1].bandX + 20, exact) == 1910 && exact == 1);
  const float middle = (t.events[0].bandX + t.events[1].bandX) / 2;
  CHECK(timelineYearAt(t, middle, exact) == 1905 && exact == -1);
  CHECK(timelineYearAt(t, -500, exact) == 1900 && exact == 0);
  CHECK(timelineYearAt(t, 1e7f, exact) == 2900 && exact == 5);

  // No year 0 while crossing from BC to AD
  const std::string acrossZero =
      "ZTL1\tdemo\tQ1\tA\tB\t2\t0\n"
      "Q1\t-1\t-\t0\ta\ta\t0\t0\t0\t0\t-\t-\n"
      "Q2\t1\t-\t0\tb\tb\t0\t0\t0\t0\t-\t-\n";
  CHECK(parseText(acrossZero, "demo", t, s));
  for (float x = t.events[0].bandX; x <= t.events[1].bandX; x += 1) CHECK(timelineYearAt(t, x, exact) != 0);

  // Deep time doesn't overflow, one event is fine
  const std::string deep =
      "ZTL1\tdemo\tQ1\tA\tB\t2\t0\n"
      "Q1\t-2147483647\t-\t0\ta\ta\t0\t0\t0\t0\t-\t-\n"
      "Q2\t2147483647\t-\t0\tb\tb\t0\t0\t0\t0\t-\t-\n";
  CHECK(parseText(deep, "demo", t, s));
  CHECK(t.events[1].bandX > 0);
  CHECK(parseText("ZTL1\tdemo\tQ1\tA\tB\t1\t0\nQ1\t5\t-\t0\ta\ta\t0\t0\t0\t0\t-\t-\n", "demo", t, s));
  CHECK(t.eventCount == 1 && t.events[0].bandX == 0);
  CHECK(timelineNearest(t, 999) == 0);
}

static void testMotion() {
  // Arrives exactly, in both directions, and a long gap takes longer than a short one
  const auto timeTo = [](float from, float to) {
    float p = from, seconds = 0;
    for (int i = 0; i < 2000 && p != to; i++) {
      p = timelineMove(p, to, 0.04f);
      seconds += 0.04f;
    }
    CHECK(p == to);
    return seconds;
  };
  const float shortTrip = timeTo(0, 264);
  const float longTrip = timeTo(0, 960);
  CHECK(longTrip > shortTrip);
  CHECK(longTrip < 2.0f);
  CHECK(timeTo(960, 0) == longTrip);
  CHECK(timelineMove(5, 5, 0.04f) == 5);
}

static void testTilt() {
  const float dt = 0.04f;
  const auto run = [&](TimelineTilt& s, float dps, float seconds) {
    int steps = 0;
    for (float t = 0; t < seconds; t += dt) steps += timelineTiltUpdate(s, dps, dt);
    return steps;
  };
  // Tip one way and back: one step only
  TimelineTilt s = {};
  CHECK(run(s, 80, 0.4f) == 1);
  CHECK(run(s, -80, 0.4f) == 0);
  CHECK(run(s, 0, 0.5f) == 0);
  // Rested, so the other way counts now
  CHECK(run(s, -80, 0.4f) == -1);
  CHECK(run(s, 80, 0.4f) == 0);
  // Slow drift and sensor noise never step
  s = {};
  CHECK(run(s, 10, 60) == 0);
  s = {};
  int total = 0;
  for (int i = 0; i < 1000; i++) total += abs(timelineTiltUpdate(s, i % 2 ? 12.0f : -12.0f, dt));
  CHECK(total == 0);
  // Shaking back and forth, above the dead zone but too small a turn each way
  s = {};
  total = 0;
  for (int i = 0; i < 1000; i++) total += abs(timelineTiltUpdate(s, i % 10 < 5 ? 40.0f : -40.0f, dt));
  CHECK(total == 0);
  // Below the step angle no step, over it a step
  s = {};
  CHECK(run(s, 100, 0.12f) == 0);
  CHECK(run(s, 100, 0.12f) == 1);
}

static int fixedWidth(const char* text, void*) {
  int n = 0;
  for (; *text; text++) {
    if (((unsigned char)*text & 0xC0) != 0x80) n++;
  }
  return n * 8;
}

static void testWrap() {
  char a[80], b[80];
  const auto wrap = [&](const char* text, int width) {
    timelineWrapLabel(text, width, fixedWidth, nullptr, a, b, sizeof(a));
  };
  wrap("Apollo 11", 232);
  CHECK_STR(a, "Apollo 11");
  CHECK_STR(b, "");
  wrap("Assassination of Archduke Franz Ferdinand", 232);  // 29 characters a line
  CHECK_STR(a, "Assassination of Archduke");
  CHECK_STR(b, "Franz Ferdinand");
  wrap("Kreide-Pal\xC3\xA4ogen-Massenaussterben", 232);
  CHECK_STR(a, "Kreide-Pal\xC3\xA4ogen-");
  CHECK_STR(b, "Massenaussterben");
  wrap("Donaudampfschifffahrtsgesellschaftskapitaensmuetzenabzeichenhalter", 80);  // One long word
  CHECK(fixedWidth(a, nullptr) <= 80 && fixedWidth(b, nullptr) <= 80);
  CHECK(strlen(b) >= 3 && strcmp(b + strlen(b) - 3, "...") == 0);
  wrap("\xC3\xA4\xC3\xB6\xC3\xBC\xC3\xA4\xC3\xB6\xC3\xBC\xC3\xA4\xC3\xB6\xC3\xBC\xC3\xA4\xC3\xB6\xC3\xBC", 40);  // Only two-byte letters
  CHECK(timelineValidText(a, 64, true) || strlen(a) == 0);
  CHECK(strlen(a) % 2 == 0);  // Never half a character
  wrap("Three words here and many more words beyond the second line of this label", 80);
  CHECK(fixedWidth(a, nullptr) <= 80 && fixedWidth(b, nullptr) <= 80);
  wrap("", 80);
  CHECK_STR(a, "");
}

static void testGeneratedFiles(const std::string& dir) {
  int topics = 0, images = 0, withEnd = 0;
  size_t largest = 0, largestImage = 0, totalImage = 0;
  for (const char* id : TIMELINE_TOPICS) {
    topics++;
    std::string text = readFile(dir + "/" + id + ".txt");
    const std::string bundle = readFile(dir + "/" + id + ".jpgs");
    Timeline t;
    std::vector<char> s;
    CHECK(text.size() < TIMELINE_TEXT_LIMIT);
    if (!parseText(text, id, t, s)) {
      printf("FAIL can't parse %s\n", id);
      failures++;
      continue;
    }
    CHECK(t.eventCount >= 3);
    CHECK(t.bundleSize == bundle.size());
    int listed = 0;
    for (int i = 0; i < t.eventCount; i++) {
      if (t.events[i].hasImage) listed++;
      if (t.events[i].endYear) withEnd++;
      CHECK(t.events[i].qid[0] == 'Q');
    }
    const int kept = timelineCheckImages(t, (const uint8_t*)bundle.data(), bundle.size());
    CHECK(kept == listed);
    images += kept;
    for (int i = 0; i < t.eventCount; i++) {
      if (!t.events[i].hasImage) continue;
      totalImage += t.events[i].imageLength;
      if (t.events[i].imageLength > largestImage) largestImage = t.events[i].imageLength;
    }
    if (bundle.size() > largest) largest = bundle.size();
    float last = -1;
    for (int i = 0; i < t.eventCount; i++) {
      CHECK(t.events[i].bandX > last);
      last = t.events[i].bandX;
    }
    // Serves as the check that the file names match the list: no others are referenced
    std::vector<char> copy(text.begin(), text.end());
    copy.resize(text.size() + 1);
    Timeline other;
    if (strcmp(id, "world_wars") != 0) CHECK(!timelineParse(copy.data(), text.size(), copy.size(), "world_wars", other));
  }
  // Every generated timeline is on the list
  if (DIR* d = opendir(dir.c_str())) {
    while (dirent* entry = readdir(d)) {
      const std::string name = entry->d_name;
      const size_t dot = name.rfind(".txt");
      if (dot != std::string::npos && dot == name.size() - 4) CHECK(timelineIsTopic(name.substr(0, dot).c_str()));
    }
    closedir(d);
  }
  printf("%d timelines, %d pictures, average %zu bytes, largest picture %zu, largest bundle %zu; %d events with an end\n",
         topics, images, images ? totalImage / images : 0, largestImage, largest, withEnd);
}

int main(int argc, char** argv) {
  testParseGood();
  testParseHostile();
  testImages();
  testTopics();
  testYears();
  testLayout();
  testMotion();
  testTilt();
  testWrap();
  if (argc > 1) testGeneratedFiles(argv[1]);
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
