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

  // Places: the real reply with ?places=1 (tests/iss/fixtures/track_places.json, made from the NASA file)
  static IssTrack p;
  CHECK(parse(readFile("fixtures/track_places.json"), p));
  CHECK(p.placeCount == 52 && p.placeChanges == 81);
  CHECK(strcmp(issPlaceAt(p, (double)p.t0, true), "Nordpazifik") == 0);
  CHECK(strcmp(issPlaceAt(p, (double)p.t0 + 109.9, false), "North Pacific") == 0);
  CHECK(strcmp(issPlaceAt(p, (double)p.t0 + 110, true), "S\xC3\xBC" "dpazifik") == 0);
  CHECK(strcmp(issPlaceAt(p, (double)p.t0 + 1135, true), "Chile") == 0);
  CHECK(issPlaceAt(p, (double)p.t0 + 1140, true) == nullptr);   // A stretch without a name (-1)
  CHECK(strcmp(issPlaceAt(p, (double)p.t0 + 1150, true), "Chile") == 0);
  CHECK(issPlaceAt(p, (double)p.t0 - 1, true) == nullptr);      // Before the track
  CHECK(issPlaceAt(p, (double)p.t0 + 60.0 * 225 + 1, true) == nullptr);  // After it, like the position
  CHECK(issPlaceAt(t, (double)t.t0 + 30, true) == nullptr);     // A reply without places
  // \u escapes are read too
  std::string esc = R"({"t0":1794122880,"step":60,"lat":[0,0,0,0,0,0,0,0,0,0],"lon":[0,1,2,3,4,5,6,7,8,9],)"
                    R"("alt":[420,420,420,420,420,420,420,420,420,420],"speed_kmh":27500,"period_min":92.9,)";
  CHECK(parse(esc + R"("places":[["Südpazifik","South Pacific"]],"place_t":[0],"place_i":[0]})", p));
  CHECK(p.placeCount == 1 && strcmp(issPlaceAt(p, 1794122880.0 + 5, true), "S\xC3\xBC" "dpazifik") == 0);
  // Bad place data: the track still counts, the places are dropped
  const char* badPlaces[] = {
      R"("places":[["A","A"]],"place_t":[0,0],"place_i":[0,0]})",           // Times not increasing
      R"("places":[["A","A"]],"place_t":[0],"place_i":[1]})",               // Index past the names
      R"("places":[["A","A"]],"place_t":[0],"place_i":[-2]})",
      R"("places":[["A","A"]],"place_t":[0,600],"place_i":[0,0]})",         // After the end of the track
      R"("places":[["A","A"]],"place_t":[-5],"place_i":[0]})",
      R"("places":[["A","A"]],"place_t":[0,10],"place_i":[0]})",            // Lengths differ
      R"("places":[["A"]],"place_t":[0],"place_i":[0]})",                   // Not a pair
      R"("places":[["","A"]],"place_t":[0],"place_i":[0]})",                // Empty name
      R"("places":[["A\u0007B","A"]],"place_t":[0],"place_i":[0]})",        // Control character
      R"("places":[["中","A"]],"place_t":[0],"place_i":[0]})",          // Not in the stick's font
      R"("places":[["0123456789012345678901234567890123456789012345678","A"]],"place_t":[0],"place_i":[0]})",  // 49 bytes
      R"("places":"Korsika","place_t":[0],"place_i":[0]})",
      R"("places":[["A","A"]],"place_t":["0"],"place_i":[0]})",
  };
  for (const char* b : badPlaces) {
    CHECK(parse(esc + b, p));
    CHECK(p.placeCount == 0 && p.placeChanges == 0);
    CHECK(issPlaceAt(p, 1794122880.0 + 5, true) == nullptr);
  }
  std::string many = esc + R"("places":[)";
  for (int i = 0; i <= ISS_MAX_PLACES; i++) many += i ? R"(,["A","A"])" : R"(["A","A"])";
  many += R"(],"place_t":[0],"place_i":[0]})";
  CHECK(parse(many, p) && p.placeCount == 0);                       // More names than the stick keeps

  // The answer names the place under the ISS when there is one
  issAnswer(out, sizeof out, true, 42.2f, 9.1f, 420.0f, 27600, 92.7f, "Korsika");
  CHECK(strcmp(out, "Die ISS fliegt gerade 420 km hoch bei 42,2\xC2\xB0 N, 9,1\xC2\xB0 O, mit 27 600 km/h. Eine Runde um die Erde dauert 93 Minuten. Unter ihr liegt gerade: Korsika.") == 0);
  issAnswer(out, sizeof out, false, 42.2f, 9.1f, 420.0f, 27600, 92.7f, "Corsica");
  CHECK(strcmp(out, "The ISS is 420 km up right now, at 42.2\xC2\xB0 N, 9.1\xC2\xB0 E, flying 27 600 km/h. One trip around the Earth takes 93 minutes. Right below it: Corsica.") == 0);

  printf("iss core: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
