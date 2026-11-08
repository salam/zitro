// Every surprise a shake can show must open: its kind is known and its id is in that feature's list.
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include "../../sketch/sketch_jan12a/shake_core.h"
#include "../../sketch/sketch_jan12a/clock_core.h"
#include "../../sketch/sketch_jan12a/model_core.h"
#include "../../sketch/sketch_jan12a/physics_core.h"
#include "../../sketch/sketch_jan12a/solid_core.h"
#include "../../sketch/sketch_jan12a/sky_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static bool opens(const char* text) {
  ModelRef ref;
  if (!parseModelRef(text, ref)) return false;
  if (strcmp(ref.kind, "physics") == 0) {
    int scene, element;
    return physicsParseQuery(ref.query, scene, element);
  }
  if (strcmp(ref.kind, "solid") == 0) return solidFindId(ref.query) >= 0;
  if (strcmp(ref.kind, "sky") == 0) return skyFindConstellation(ref.query) != SKY_NOT_FOUND;
  if (strcmp(ref.kind, "clock") == 0) return clockFindCity(ref.query) >= 0;
  return false;  // Anything else could need the network: not for a shake
}

int main() {
  std::set<std::string> seen;
  for (uint32_t pick = 0; pick < (uint32_t)SHAKE_MEDIA_COUNT * 3; pick++) {
    for (uint32_t city = 0; city < 200; city += 7) {
      char ref[80];
      CHECK(shakePickMedia(pick, city, ref, sizeof ref));
      const bool ok = opens(ref);
      if (!ok) fprintf(stderr, "  does not open: %s\n", ref);
      CHECK(ok);
      seen.insert(ref);
    }
  }
  // Every entry comes up, and the clock picks many cities
  CHECK((int)seen.size() >= SHAKE_MEDIA_COUNT - 1 + 20);
  char tiny[4];
  CHECK(!shakePickMedia(0, 0, tiny, sizeof tiny));
  printf("shake: %d checks, %d failures, %zu different surprises\n", checks, failures, seen.size());
  return failures ? 1 : 0;
}
