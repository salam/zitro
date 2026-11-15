// Host tests for the worries rule (care_core.h). Run: tests/care/run.sh
#include <cstdio>
#include <string>
#include "../../sketch/sketch_jan12a/care_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
static bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

int main() {
  // Every rule sends the child to people and keeps pictures away
  for (const char* zone : {"Europe/Zurich", "Europe/Vienna", "Europe/Berlin", "Asia/Tokyo", "", (const char*)nullptr}) {
    const std::string r = careRuleText(zone);
    CHECK(has(r, "Eltern"));
    CHECK(has(r, "Lehrperson"));
    CHECK(has(r, "image_query leer"));
    CHECK(has(r, "keine anderen Telefonnummern"));
    CHECK(r.find('\n') == std::string::npos && r.find('"') == std::string::npos);  // Goes into a JSON string
  }

  // Switzerland: Pro Juventute
  std::string ch = careRuleText("Europe/Zurich");
  CHECK(has(ch, "147 (Pro Juventute"));
  CHECK(has(ch, "112"));
  // Austria: Rat auf Draht, Germany: Nummer gegen Kummer
  CHECK(has(careRuleText("Europe/Vienna"), "147 (Rat auf Draht"));
  CHECK(has(careRuleText("Europe/Berlin"), "116 111 (Nummer gegen Kummer"));
  CHECK(!has(careRuleText("Europe/Berlin"), "147"));
  // Elsewhere: no number at all, an adult instead
  std::string jp = careRuleText("Asia/Tokyo");
  CHECK(!has(jp, "147") && !has(jp, "116") && !has(jp, "112"));
  CHECK(has(jp, "Erwachsenen zu holen"));
  CHECK(careHelpline("America/New_York") == nullptr);
  CHECK(careHelpline(nullptr) == nullptr);

  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
