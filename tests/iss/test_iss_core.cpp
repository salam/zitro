// Host tests for the stick's side of the live ISS view (iss_core.h). Run: tests/iss/run.sh
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../../sketch/sketch_jan12a/iss_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, e) CHECK(fabs((a) - (b)) <= (e))

static std::string readFile(const char* path) {
  std::string out;
  FILE* f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "missing %s\n", path); exit(2); }
  char buf[4096]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
  fclose(f);
  return out;
}
static bool parse(const std::string& s, IssTrack& t) { return issParseTrack(s.c_str(), s.size(), t); }

int main() {
  static IssTrack t;
  const std::string real = readFile("fixtures/track.json");
  CHECK(parse(real, t));
  CHECK(t.count == 226 && t.step == 60);
  CHECK(t.speedKmh > 27000 && t.speedKmh < 28000);
  float lat, lon, alt;
  CHECK(issPositionAt(t, (double)t.t0, lat, lon, alt));
  NEAR(lat, t.lat100[0] / 100.0, 1e-4);
  CHECK(issPositionAt(t, (double)t.t0 + 90, lat, lon, alt));  // Between points 1 and 2
  NEAR(lat, (t.lat100[1] + t.lat100[2]) / 200.0, 0.01);
  CHECK(alt > 380 && alt < 450);
  CHECK(!issPositionAt(t, (double)t.t0 - 1, lat, lon, alt));
  CHECK(!issPositionAt(t, (double)t.t0 + 60.0 * 226, lat, lon, alt));
  CHECK(issPositionAt(t, (double)t.t0 + 60.0 * 225, lat, lon, alt));  // The very last point

  // Across the date line: 179.5 to -179.5 goes through 180, not back round the world
  std::string dateline = R"({"t0":1794122880,"step":60,"lat":[0,0,0,0,0,0,0,0,0,0],"lon":[179.5,-179.5,-178.5,-177.5,-176.5,-175.5,-174.5,-173.5,-172.5,-171.5],)"
                         R"("alt":[420,420,420,420,420,420,420,420,420,420],"speed_kmh":27500,"period_min":92.9})";
  CHECK(parse(dateline, t));
  CHECK(issPositionAt(t, 1794122880.0 + 30, lat, lon, alt));
  NEAR(fabs(lon), 180.0, 0.01);
  CHECK(issPositionAt(t, 1794122880.0 + 15, lat, lon, alt));
  NEAR(lon, 179.75, 0.01);

  // Anything wrong: refused
  const char* bad[] = {
      "", "{}", "[]", "not json",
      R"({"t0":1794122880,"step":60,"lat":[0,0],"lon":[0,0],"alt":[420,420],"speed_kmh":27500,"period_min":92.9})",  // Too short
      R"({"t0":1794122880,"step":60,"lat":[0,0,0,0,0,0,0,0,0,95],"lon":[0,0,0,0,0,0,0,0,0,0],"alt":[420,420,420,420,420,420,420,420,420,420],"speed_kmh":27500,"period_min":92.9})",
      R"({"t0":1794122880,"step":60,"lat":[0,0,0,0,0,0,0,0,0,0],"lon":[0,0,0,0,0,0,0,0,0],"alt":[420,420,420,420,420,420,420,420,420,420],"speed_kmh":27500,"period_min":92.9})",
      R"({"t0":1794122880,"step":60,"lat":[0,0,0,0,0,0,0,0,0,0],"lon":[0,0,0,0,0,0,0,0,0,0],"alt":[420,420,420,420,420,420,420,420,420,5],"speed_kmh":27500,"period_min":92.9})",
      R"({"t0":1794122880,"step":0,"lat":[0,0,0,0,0,0,0,0,0,0],"lon":[0,0,0,0,0,0,0,0,0,0],"alt":[420,420,420,420,420,420,420,420,420,420],"speed_kmh":27500,"period_min":92.9})",
      R"({"t0":100,"step":60,"lat":[0,0,0,0,0,0,0,0,0,0],"lon":[0,0,0,0,0,0,0,0,0,0],"alt":[420,420,420,420,420,420,420,420,420,420],"speed_kmh":27500,"period_min":92.9})",
      R"({"t0":1794122880,"step":60,"lat":[0,0,0,0,0,0,0,0,0,0],"lon":[0,0,0,0,0,0,0,0,0,0],"alt":[420,420,420,420,420,420,420,420,420,420],"speed_kmh":5,"period_min":92.9})",
      R"({"t0":1794122880,"step":60,"lat":["x",0,0,0,0,0,0,0,0,0],"lon":[0,0,0,0,0,0,0,0,0,0],"alt":[420,420,420,420,420,420,420,420,420,420],"speed_kmh":27500,"period_min":92.9})",
      R"({"error":"trajectory data does not cover now"})",
  };
  for (const char* b : bad) CHECK(!parse(b, t));
  std::string big = R"({"t0":1794122880,"step":60,"lat":[)";
  for (int i = 0; i < 500; i++) big += i ? ",0" : "0";
  big += "]}";
  CHECK(!parse(big, t));

  // The answer
  char out[256];
  issAnswer(out, sizeof out, true, -25.26f, -22.29f, 430.3f, 27544, 92.9f);
  CHECK(strcmp(out, "Die ISS fliegt gerade 430 km hoch bei 25,3\xC2\xB0 S, 22,3\xC2\xB0 W, mit 27 544 km/h. Eine Runde um die Erde dauert 93 Minuten.") == 0);
  issAnswer(out, sizeof out, false, 51.5f, 145.2f, 418.0f, 27600, 92.7f);
  CHECK(strcmp(out, "The ISS is 418 km up right now, at 51.5\xC2\xB0 N, 145.2\xC2\xB0 E, flying 27 600 km/h. One trip around the Earth takes 93 minutes.") == 0);
  issFormatThousands(out, sizeof out, 950);
  CHECK(strcmp(out, "950") == 0);
  issFormatThousands(out, sizeof out, 27005);
  CHECK(strcmp(out, "27 005") == 0);
  issNoAnswer(out, sizeof out, true, true);
  CHECK(strstr(out, "Zitro-Server") != nullptr);

  printf("iss core: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
