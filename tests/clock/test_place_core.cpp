// Host tests for the stick's side of the place lookup: tests/clock/run.sh
// Arguments: the server's zones.tsv, and a file of lines "name<TAB>folded" written by the Python fold.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include "../../sketch/sketch_jan12a/clock_place.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int64_t utcOf(int y, int mo, int d, int h, int mi) { return clockDaysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60; }

static ClockPlaceResult parse(const std::string& body, ClockPlace& p) { return clockParsePlace(body.c_str(), body.size(), p); }

static const char* GOOD =
    "{\"found\":true,\"name\":\"Knoxville\",\"region\":\"TN\",\"country\":\"US\",\"lat\":35.961,\"lon\":-83.921,"
    "\"zone\":\"America/New_York\",\"rule\":\"EST5EDT,M3.2.0,M11.1.0\",\"source\":\"GeoNames\"}";

static void testReplies() {
  ClockPlace p;
  CHECK(parse(GOOD, p) == CLOCK_PLACE_FOUND);
  CHECK(!strcmp(p.name, "Knoxville") && !strcmp(p.label, "Knoxville, TN"));
  CHECK(fabs(p.lat - 35.961f) < 0.001f && fabs(p.lon + 83.921f) < 0.001f);
  CHECK(clockUtcOffset(p.rule, utcOf(2026, 11, 9, 12, 0)) == -5 * 3600);
  CHECK(clockUtcOffset(p.rule, utcOf(2026, 7, 1, 12, 0)) == -4 * 3600);

  CHECK(parse("{\"found\":false}", p) == CLOCK_PLACE_UNKNOWN);

  std::string good = GOOD;
  const auto with = [&](const char* from, const std::string& to) {
    std::string s = good;
    const size_t at = s.find(from);
    CHECK(at != std::string::npos);
    return s.replace(at, strlen(from), to);
  };
  for (const std::string& bad : {
           std::string(""), std::string("not json"), std::string("{}"), std::string("[1,2]"), std::string("{\"found\":\"yes\"}"),
           good.substr(0, good.size() / 2),
           with("\"lat\":35.961", "\"lat\":91"), with("\"lat\":35.961", "\"lat\":\"x\""), with("\"lat\":35.961,", ""),
           with("\"lon\":-83.921", "\"lon\":181"),
           with("\"name\":\"Knoxville\"", "\"name\":\"\""), with("\"name\":\"Knoxville\"", "\"name\":\"a\\nb\""),
           with("\"name\":\"Knoxville\"", "\"name\":\"" + std::string(64, 'a') + "\""), with("\"name\":\"Knoxville\",", ""),
           with("\"region\":\"TN\"", "\"region\":\"TOOLONGREGION\""), with("\"region\":\"TN\",", ""),
           with("EST5EDT,M3.2.0,M11.1.0", "banana"), with("EST5EDT,M3.2.0,M11.1.0", ""),
           with("EST5EDT,M3.2.0,M11.1.0", "EST5EDT"), with("EST5EDT,M3.2.0,M11.1.0", std::string(70, 'A') + "5"),
       }) {
    CHECK(parse(bad, p) == CLOCK_PLACE_BAD);
  }
  // A format string in a name is only ever text
  CHECK(parse(with("\"name\":\"Knoxville\"", "\"name\":\"%s%n\""), p) == CLOCK_PLACE_FOUND);
  CHECK(!strcmp(p.label, "%s%n, TN"));
  // The longest allowed name and region still end in a whole label
  CHECK(parse(with("\"name\":\"Knoxville\"", "\"name\":\"" + std::string(63, 'a') + "\""), p) == CLOCK_PLACE_FOUND);
  CHECK(strlen(p.label) == 67);
}

static void testSplit() {
  char n[64], h[64];
  CHECK(clockSplitPlaceQuery("Knoxville", n, sizeof(n), h, sizeof(h)) && !strcmp(n, "knoxville") && !h[0]);
  CHECK(clockSplitPlaceQuery("Paris, Texas", n, sizeof(n), h, sizeof(h)) && !strcmp(n, "paris") && !strcmp(h, "texas"));
  CHECK(clockSplitPlaceQuery("St. Gallen,", n, sizeof(n), h, sizeof(h)) && !strcmp(n, "stgallen") && !h[0]);
  CHECK(clockSplitPlaceQuery("Göttingen, Deutschland", n, sizeof(n), h, sizeof(h)) && !strcmp(n, "gottingen") && !strcmp(h, "deutschland"));
  CHECK(!clockSplitPlaceQuery("", n, sizeof(n), h, sizeof(h)));
  CHECK(!clockSplitPlaceQuery("x", n, sizeof(n), h, sizeof(h)));
  CHECK(!clockSplitPlaceQuery("東京", n, sizeof(n), h, sizeof(h)));
  CHECK(!clockSplitPlaceQuery("Paris, x", n, sizeof(n), h, sizeof(h)));
  CHECK(!clockSplitPlaceQuery(std::string(49, 'a').c_str(), n, sizeof(n), h, sizeof(h)));
  CHECK(clockSplitPlaceQuery(std::string(48, 'a').c_str(), n, sizeof(n), h, sizeof(h)));
  // The whole name goes to the built-in table first: Paris, Texas is not the Paris in the table
  CHECK(clockFindCity("Paris") >= 0);
  CHECK(clockFindCity("Paris, Texas") < 0);
}

static void testAnswer() {
  ClockPlace p;
  CHECK(parse(GOOD, p) == CLOCK_PLACE_FOUND);
  ClockRule zurich;
  const int home = clockFindCity("Zurich");
  CHECK(clockZoneRule(CLOCK_CITIES[home].zone, zurich));
  char text[256];
  clockAnswerFor(text, sizeof(text), true, p.label, p.rule, false, clockCityName(home, true), zurich, utcOf(2026, 11, 9, 7, 45));
  CHECK(!strcmp(text, "In Knoxville, TN ist es jetzt 02:45 Uhr, Montag. Das ist 6 Stunden früher als in Zürich."));
  clockAnswerFor(text, sizeof(text), false, p.label, p.rule, false, clockCityName(home, false), zurich, utcOf(2026, 11, 9, 7, 45));
  CHECK(text == std::string("In Knoxville, TN it is 02:45 now, Monday. That is 6 hours earlier than in ") + clockCityName(home, false) + ".");
  clockNoServerAnswer(text, sizeof(text), true, "Knoxville");
  CHECK(strstr(text, "Knoxville") && strstr(text, "Zitro-Server"));
  // The longest place still leaves a whole sentence in the screen's buffer
  CHECK(parse(std::string(GOOD).replace(std::string(GOOD).find("Knoxville"), 9, std::string(63, 'a')), p) == CLOCK_PLACE_FOUND);
  char big[256];
  clockAnswerFor(big, sizeof(big), true, p.label, p.rule, false, clockCityName(home, true), zurich, utcOf(2026, 11, 9, 7, 45));
  CHECK(strlen(big) < sizeof(big) - 1 && strstr(big, "Zürich."));
}

// Every rule the server can send has to work here, and agree with this computer's tz database
static void testServerZones(const char* path) {
  std::ifstream file(path);
  CHECK(file.good());
  std::string line;
  int zones = 0, compared = 0, wrong = 0;
  while (std::getline(file, line)) {
    const size_t tab = line.find('\t');
    CHECK(tab != std::string::npos);
    const std::string iana = line.substr(0, tab), posix = line.substr(tab + 1);
    ClockRule r;
    const bool parsed = clockParseRule(posix.c_str(), r);
    CHECK(parsed);
    CHECK(posix.size() < 64);
    if (!parsed) {
      fprintf(stderr, "  zone %s: can't read %s\n", iana.c_str(), posix.c_str());
      continue;
    }
    zones++;
    setenv("TZ", (":" + iana).c_str(), 1);
    tzset();
    for (int64_t t = utcOf(2026, 11, 1, 0, 0); t < utcOf(2030, 11, 1, 0, 0); t += 3 * 3600) {
      time_t tt = (time_t)t;
      struct tm local;
      localtime_r(&tt, &local);
      compared++;
      if (local.tm_gmtoff != clockUtcOffset(r, t) && wrong++ < 8) {
        fprintf(stderr, "  zone %s at %lld: system %ld, ours %d\n", iana.c_str(), (long long)t, (long)local.tm_gmtoff, clockUtcOffset(r, t));
      }
    }
  }
  unsetenv("TZ");
  tzset();
  checks++;
  if (wrong) failures++;
  printf("  server zones: %d zones, %d comparisons, %d differ\n", zones, compared, wrong);
}

// The server's keys are folded by Python; the stick folds what it asks for. They have to agree.
static void testFold(const char* path) {
  std::ifstream file(path);
  CHECK(file.good());
  std::string line;
  int compared = 0, wrong = 0;
  while (std::getline(file, line)) {
    const size_t tab = line.find('\t');
    char folded[256];
    clockFold(line.substr(0, tab).c_str(), folded, sizeof(folded));
    compared++;
    if (line.substr(tab + 1) != folded && wrong++ < 8) fprintf(stderr, "  fold '%s': python %s, stick %s\n", line.substr(0, tab).c_str(), line.substr(tab + 1).c_str(), folded);
  }
  checks++;
  if (wrong || compared == 0) failures++;
  printf("  fold: %d names, %d differ\n", compared, wrong);
}

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  testReplies();
  testSplit();
  testAnswer();
  testServerZones(argv[1]);
  testFold(argv[2]);
  printf("place core: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
